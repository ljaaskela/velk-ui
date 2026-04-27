#include "render_context.h"

#include "shader.h"
#include "shader_compiler.h"
#include "spirv_material_reflect.h"
#include "surface.h"

#include <velk/api/perf.h>
#include <velk/api/state.h>
#include <velk/api/velk.h>
#include <velk/hash.h>

#include <velk-render/frame/draw_call_emit.h>
#include <velk-render/frame/raster_shaders.h>
#include <velk-render/gpu_data.h>
#include <velk-render/interface/intf_buffer.h>
#include <velk-render/interface/material/intf_material_options.h>
#include <velk-render/interface/intf_render_target.h>
#include <velk-render/interface/intf_surface.h>
#include <velk-render/interface/material/intf_material_internal.h>
#include <velk-render/platform.h>

#include <cstring>

namespace velk {

namespace {

// Snapshots the IMaterialOptions attached to a material into a
// PipelineOptions. Caller fills `topology` from the primitive.
PipelineOptions pipeline_options_from_storage(IObjectStorage* storage)
{
    PipelineOptions o{};
    if (!storage) return o;
    auto opts = storage->template find_attachment<IMaterialOptions>();
    if (!opts) return o;
    auto r = read_state<IMaterialOptions>(opts.get());
    if (!r) return o;
    o.cull_mode = r->cull_mode;
    o.front_face = r->front_face;
    o.blend_mode = (r->alpha_mode == AlphaMode::Blend)
                       ? BlendMode::Alpha
                       : BlendMode::Opaque;
    o.depth_test = r->depth_test;
    o.depth_write = r->depth_write;
    return o;
}

Topology to_backend_topology(MeshTopology mt)
{
    return mt == MeshTopology::TriangleStrip
             ? Topology::TriangleStrip
             : Topology::TriangleList;
}

} // namespace

bool RenderContextImpl::init(const RenderConfig& config)
{
    RenderBackendType backend_type = config.backend;
    if (backend_type == RenderBackendType::Default) {
        backend_type = RenderBackendType::Vulkan;
    }

    Uid plugin_id;
    Uid class_id;

    switch (backend_type) {
    case RenderBackendType::Vulkan:
        plugin_id = PluginId::VkPlugin;
        class_id = ClassId::VkBackend;
        break;
    default:
        VELK_LOG(E, "RenderContext::init: unsupported backend type %d", static_cast<int>(backend_type));
        return false;
    }

    auto& reg = instance().plugin_registry();
    if (!reg.get_or_load_plugin(plugin_id)) {
        VELK_LOG(E, "RenderContext::init: failed to load backend plugin");
        return false;
    }

    auto obj = instance().create<IObject>(class_id);
    backend_ = interface_pointer_cast<IRenderBackend>(obj);
    if (!backend_) {
        VELK_LOG(E, "RenderContext::init: failed to create backend");
        return false;
    }

    if (!backend_->init(config.backend_params)) {
        VELK_LOG(E, "RenderContext::init: backend init failed");
        backend_ = nullptr;
        return false;
    }

    // Register the framework-level velk.glsl include so it appears in
    // shader_includes_ alongside any plugin-registered includes. The shader
    // cache uses this map to compute its per-shader cache keys.
    shader_includes_["velk.glsl"] = string(kVelkGlsl);

    mesh_builder_ = instance().create<IMeshBuilder>(ClassId::MeshBuilder);
    if (!mesh_builder_) {
        VELK_LOG(E, "RenderContext::init: failed to create mesh builder");
        backend_ = nullptr;
        return false;
    }

    // Default UV1 buffer: one vec2(0, 0) shared by every draw whose
    // primitive has no TEXCOORD_1. The Renderer uploads it once at
    // set_backend time; `velk_uv1` reads index 0 via
    // DrawDataHeader::uv1_enabled == 0.
    {
        const float zero_uv1[2] = {0.f, 0.f};
        default_uv1_ = mesh_builder_->build_buffer(zero_uv1, sizeof(zero_uv1), nullptr, 0);
        if (!default_uv1_) {
            VELK_LOG(E, "RenderContext::init: failed to create default uv1 buffer");
            backend_ = nullptr;
            return false;
        }
    }

    initialized_ = true;
    VELK_LOG(I, "RenderContext initialized (Vulkan, bindless)");
    return true;
}

IWindowSurface::Ptr RenderContextImpl::create_surface(const SurfaceConfig& config)
{
    auto obj = instance().create<IObject>(WindowSurface::static_class_id());
    auto surface = interface_pointer_cast<IWindowSurface>(obj);
    if (!surface) {
        return nullptr;
    }

    write_state<IWindowSurface>(surface, [&](IWindowSurface::State& s) {
        s.size = {static_cast<uint32_t>(config.width), static_cast<uint32_t>(config.height)};
        s.update_rate = config.update_rate;
        s.target_fps = config.target_fps;
    });

    surface->set_depth_format(config.depth);

    // Eagerly create the backend-side swapchain so the backend's default
    // render pass gets upgraded to match the surface's depth config
    // before any pipelines are compiled. Otherwise a pipeline compiled
    // between create_surface() and add_view() would target the initial
    // depth-less default render pass and be incompatible with the
    // swapchain's depth-enabled render pass at draw time.
    if (backend_) {
        SurfaceDesc desc{};
        desc.width = config.width;
        desc.height = config.height;
        desc.update_rate = config.update_rate;
        desc.target_fps = config.target_fps;
        desc.depth = config.depth;
        uint64_t sid = backend_->create_surface(desc);
        if (sid != 0) {
            surface->set_render_target_id(sid);
        }
    }

    return surface;
}

IMeshBuilder& RenderContextImpl::get_mesh_builder()
{
    return *mesh_builder_;
}

IBuffer::Ptr RenderContextImpl::get_default_buffer(DefaultBufferType type) const
{
    switch (type) {
    case DefaultBufferType::Uv1:
        return interface_pointer_cast<IBuffer>(default_uv1_);
    }
    return nullptr;
}

IShader::Ptr RenderContextImpl::compile_shader(string_view source, ShaderStage stage, uint64_t key)
{
    if (!initialized_ || source.empty()) {
        return nullptr;
    }

    if (key == 0) {
        key = make_hash64(source);
    }

    shader_cache_.ensure_initialized();

    // Combined cache key: source key XOR stage discriminator XOR hash of all
    // currently-registered includes. Folding the include hash into the key
    // means that any change to a virtual include (e.g. velk.glsl) naturally
    // invalidates affected entries; old entries with the previous include
    // content become orphans rather than corrupt cache hits.
    constexpr uint64_t kStageVertexMix = 0x68f3df8b8e0c8b8dULL;
    constexpr uint64_t kStageFragmentMix = 0xa24baed4963ee407ULL;
    constexpr uint64_t kStageComputeMix = 0x5a3b1d2f6e9c8411ULL;
    uint64_t stage_mix = kStageFragmentMix;
    switch (stage) {
    case ShaderStage::Vertex:   stage_mix = kStageVertexMix;   break;
    case ShaderStage::Fragment: stage_mix = kStageFragmentMix; break;
    case ShaderStage::Compute:  stage_mix = kStageComputeMix;  break;
    }
    uint64_t include_hash = hash_shader_includes(shader_includes_);
    uint64_t cache_key = key ^ stage_mix ^ include_hash;

    auto cached = shader_cache_.read(cache_key);
    if (!cached.empty()) {
        auto shader = instance().create<IShader>(Shader::static_class_id());
        if (shader) {
            shader->init(std::move(cached));
            return shader;
        }
    }

    auto* includes = shader_includes_.empty() ? nullptr : &shader_includes_;
    auto spirv = compile_glsl_to_spirv(source, stage, includes);
    if (spirv.empty()) {
        return nullptr;
    }

    shader_cache_.write(cache_key, spirv);

    auto shader = instance().create<IShader>(Shader::static_class_id());
    if (!shader) {
        return nullptr;
    }
    shader->init(std::move(spirv));
    return shader;
}

uint64_t RenderContextImpl::create_pipeline(const IShader::Ptr& vertex, const IShader::Ptr& fragment,
                                            uint64_t key, RenderTargetGroup target_group,
                                            const PipelineOptions& options)
{
    if (!initialized_ || !backend_) {
        return 0;
    }

    const auto& vert_shader = vertex ? vertex : default_vertex_shader_;
    const auto& frag_shader = fragment ? fragment : default_fragment_shader_;
    if (!vert_shader || !frag_shader) {
        VELK_LOG(E, "create_pipeline: missing vertex or fragment shader");
        return 0;
    }

    auto vert_data = vert_shader->get_data();
    auto frag_data = frag_shader->get_data();
    if (vert_data.empty() || frag_data.empty()) {
        VELK_LOG(E, "create_pipeline: empty shader bytecode");
        return 0;
    }

    PipelineDesc desc;
    desc.vertex = vert_shader;
    desc.fragment = frag_shader;
    desc.options = options;

    PipelineId pid = backend_->create_pipeline(desc, target_group);
    if (!pid) {
        return 0;
    }

    if (key == 0) {
        key = next_pipeline_key_++;
    }
    pipeline_map_[key] = pid;
    return key;
}

uint64_t RenderContextImpl::compile_pipeline(string_view fragment_source, string_view vertex_source,
                                             uint64_t key, RenderTargetGroup target_group,
                                             const PipelineOptions& options)
{
    auto vert = vertex_source.empty() ? nullptr : compile_shader(vertex_source, ShaderStage::Vertex);
    auto frag = fragment_source.empty() ? nullptr : compile_shader(fragment_source, ShaderStage::Fragment);
    return create_pipeline(vert, frag, key, target_group, options);
}

uint64_t RenderContextImpl::create_compute_pipeline(const IShader::Ptr& compute, uint64_t key)
{
    if (!initialized_ || !backend_ || !compute) {
        return 0;
    }

    ComputePipelineDesc desc;
    desc.compute = compute;

    PipelineId pid = backend_->create_compute_pipeline(desc);
    if (!pid) {
        return 0;
    }

    if (key == 0) {
        key = next_pipeline_key_++;
    }
    pipeline_map_[key] = pid;
    return key;
}

uint64_t RenderContextImpl::compile_compute_pipeline(string_view compute_source, uint64_t key)
{
    if (compute_source.empty()) {
        return 0;
    }
    auto compute = compile_shader(compute_source, ShaderStage::Compute);
    if (!compute) {
        return 0;
    }
    return create_compute_pipeline(compute, key);
}

PipelineId RenderContextImpl::compile_gbuffer_pipeline(string_view fragment_source,
                                                       string_view vertex_source,
                                                       uint64_t key,
                                                       RenderTargetGroup target_group,
                                                       const PipelineOptions& options)
{
    if (!initialized_ || !backend_ || key == 0 || target_group == 0) {
        return 0;
    }
    auto it = gbuffer_pipeline_map_.find(key);
    if (it != gbuffer_pipeline_map_.end()) {
        return it->second;
    }

    auto vert = vertex_source.empty()
                    ? default_gbuffer_vertex_shader_
                    : compile_shader(vertex_source, ShaderStage::Vertex);
    auto frag = fragment_source.empty()
                    ? default_gbuffer_fragment_shader_
                    : compile_shader(fragment_source, ShaderStage::Fragment);
    if (!vert || !frag) {
        VELK_LOG(E, "compile_gbuffer_pipeline: missing vertex or fragment shader for key %llu",
                 static_cast<unsigned long long>(key));
        return 0;
    }

    PipelineDesc desc;
    desc.vertex = vert;
    desc.fragment = frag;
    desc.options = options;
    // G-buffer passes always write opaquely regardless of alpha mode.
    desc.options.blend_mode = BlendMode::Opaque;

    PipelineId pid = backend_->create_pipeline(desc, target_group);
    if (!pid) {
        return 0;
    }
    gbuffer_pipeline_map_[key] = pid;
    return pid;
}

void RenderContextImpl::set_default_gbuffer_vertex_shader(const IShader::Ptr& shader)
{
    default_gbuffer_vertex_shader_ = shader;
}

void RenderContextImpl::set_default_gbuffer_fragment_shader(const IShader::Ptr& shader)
{
    default_gbuffer_fragment_shader_ = shader;
}

void RenderContextImpl::set_default_vertex_shader(const IShader::Ptr& shader)
{
    default_vertex_shader_ = shader;
}

void RenderContextImpl::set_default_fragment_shader(const IShader::Ptr& shader)
{
    default_fragment_shader_ = shader;
}

void RenderContextImpl::register_shader_include(string_view name, string_view content)
{
    shader_includes_[name] = content;
}

IMaterial::Ptr RenderContextImpl::create_shader_material(string_view fragment_source,
                                                         string_view vertex_source)
{
    auto mat = instance().create<IMaterialInternal>(ClassId::ShaderMaterial);
    if (!mat) {
        return nullptr;
    }

    // Hand over the sources; pipeline compilation is the renderer's job
    // and happens lazily on first draw — so any IMaterialOptions attached
    // between now and then is honored, same as every other material.
    mat->set_sources(vertex_source, fragment_source);

    // Reflect material parameters from the vertex shader SPIR-V for
    // dynamic-property setup. Compilation hits the shader cache on
    // second run (inside the renderer's pipeline compile).
    auto vert = vertex_source.empty()
                    ? default_vertex_shader_
                    : compile_shader(vertex_source, ShaderStage::Vertex);
    if (vert) {
        auto vert_data = vert->get_data();
        if (!vert_data.empty()) {
            auto params = reflect_material_params(vert_data.begin(), vert_data.size());
            if (!params.empty()) {
                mat->setup_inputs(params);
            }
        }
    }

    return mat;
}

vector<DrawCall> RenderContextImpl::build_draw_calls(
    const vector<Batch>& batches,
    IFrameDataManager& frame_data,
    IGpuResourceManager& resources,
    uint64_t globals_gpu_addr,
    IGpuResourceObserver* observer,
    MaterialAddrCache& material_cache,
    const ::velk::render::Frustum* frustum)
{
    VELK_PERF_SCOPE("renderer.build_draw_calls");

    vector<DrawCall> out_calls;

    for (auto& batch : batches) {
        if (frustum && !::velk::render::aabb_in_frustum(*frustum, batch.world_aabb)) {
            continue;
        }
        uint64_t instances_addr =
            frame_data.write(batch.instance_data.data(), batch.instance_data.size());
        if (!instances_addr) {
            continue;
        }

        uint32_t texture_id = 0;
        if (batch.texture_key != 0) {
            auto* tex = reinterpret_cast<ISurface*>(batch.texture_key);
            texture_id = resources.find_texture(tex);
            if (texture_id == 0) {
                uint64_t rt_id = get_render_target_id(tex);
                if (rt_id != 0) {
                    texture_id = static_cast<uint32_t>(rt_id);
                }
            }
        }

        IMeshPrimitive* primitive = batch.primitive.get();
        if (!primitive) continue;
        auto buffer = primitive->get_buffer();
        if (!buffer) continue;

        // IBO half is optional: indexed draw when ibo_size > 0, plain
        // vkCmdDraw when 0 (e.g. TriangleStrip unit quad).
        GpuBuffer ibo_handle = 0;
        size_t ibo_offset = 0;
        if (buffer->get_ibo_size() > 0) {
            auto* buf_entry = resources.find_buffer(buffer.get());
            if (!buf_entry || !buf_entry->handle) continue;
            ibo_handle = buf_entry->handle;
            ibo_offset = buffer->get_ibo_offset();
        }

        DrawDataHeader header{};
        header.globals_address = globals_gpu_addr;
        header.instances_address = instances_addr;
        header.texture_id = texture_id;
        header.instance_count = batch.instance_count;
        header.vbo_address = buffer->get_gpu_address();
        if (!header.vbo_address) continue;

        // Resolve TEXCOORD_1: primitive's own UV1 stream if present,
        // otherwise the context-owned default (read as index 0).
        if (auto uv1 = primitive->get_uv1_buffer()) {
            uint64_t uv1_base = uv1->get_gpu_address();
            if (!uv1_base) continue;
            header.uv1_address = uv1_base + primitive->get_uv1_offset();
            header.uv1_enabled = 1;
        } else {
            auto def = get_default_buffer(DefaultBufferType::Uv1);
            header.uv1_address = def ? def->get_gpu_address() : 0;
            header.uv1_enabled = 0;
            if (!header.uv1_address) continue;
        }

        uint64_t material_addr = detail::write_material_once(
            batch.material.get(), frame_data,
            static_cast<ITextureResolver*>(&resources),
            material_cache);

        constexpr size_t kMaterialPtrSize = sizeof(uint64_t);
        size_t total_size = sizeof(DrawDataHeader) + kMaterialPtrSize;

        auto reservation = frame_data.reserve(total_size);
        if (!reservation.ptr) {
            continue;
        }

        auto* dst = static_cast<uint8_t*>(reservation.ptr);
        uint64_t draw_data_addr = reservation.gpu_addr;

        std::memcpy(dst, &header, sizeof(header));
        std::memcpy(dst + sizeof(DrawDataHeader), &material_addr, kMaterialPtrSize);

        uint64_t effective_pipeline_key = batch.pipeline_key;
        if (effective_pipeline_key == 0 && batch.material) {
            effective_pipeline_key = batch.material->get_pipeline_handle(*this);
        }
        auto pit = pipeline_map_.find(effective_pipeline_key);
        if (pit == pipeline_map_.end()) {
            continue;
        }

        // Lazy-register the program's pipeline for deferred destruction
        // on program destruction. Idempotent; subscribes observer only
        // once per program.
        if (batch.material) {
            if (resources.register_pipeline(batch.material.get(), pit->second) && observer) {
                batch.material->add_gpu_resource_observer(observer);
            }
        }

        DrawCall call{};
        call.pipeline = pit->second;
        if (ibo_handle) {
            call.index_buffer = ibo_handle;
            call.index_buffer_offset = ibo_offset;
            call.index_count = primitive->get_index_count();
        } else {
            call.vertex_count = primitive->get_vertex_count();
        }
        call.instance_count = batch.instance_count;
        call.root_constants_size = sizeof(uint64_t);
        std::memcpy(call.root_constants, &draw_data_addr, sizeof(uint64_t));

        out_calls.push_back(call);
    }

    return out_calls;
}

vector<DrawCall> RenderContextImpl::build_gbuffer_draw_calls(
    const vector<Batch>& batches,
    IFrameDataManager& frame_data,
    IGpuResourceManager& resources,
    uint64_t globals_gpu_addr,
    RenderTargetGroup target_group,
    IGpuResourceObserver* observer,
    MaterialAddrCache& material_cache,
    const ::velk::render::Frustum* frustum)
{
    VELK_PERF_SCOPE("renderer.build_gbuffer_draw_calls");

    vector<DrawCall> out_calls;
    if (target_group == 0) return out_calls;

    for (auto& batch : batches) {
        if (frustum && !::velk::render::aabb_in_frustum(*frustum, batch.world_aabb)) {
            continue;
        }
        uint64_t instances_addr =
            frame_data.write(batch.instance_data.data(), batch.instance_data.size());
        if (!instances_addr) {
            continue;
        }

        uint32_t texture_id = 0;
        if (batch.texture_key != 0) {
            auto* tex = reinterpret_cast<ISurface*>(batch.texture_key);
            texture_id = resources.find_texture(tex);
            if (texture_id == 0) {
                uint64_t rt_id = get_render_target_id(tex);
                if (rt_id != 0) {
                    texture_id = static_cast<uint32_t>(rt_id);
                }
            }
        }

        IMeshPrimitive* primitive = batch.primitive.get();
        if (!primitive) continue;
        auto buffer = primitive->get_buffer();
        if (!buffer) continue;

        GpuBuffer ibo_handle = 0;
        size_t ibo_offset = 0;
        if (buffer->get_ibo_size() > 0) {
            auto* buf_entry = resources.find_buffer(buffer.get());
            if (!buf_entry || !buf_entry->handle) continue;
            ibo_handle = buf_entry->handle;
            ibo_offset = buffer->get_ibo_offset();
        }

        DrawDataHeader header{};
        header.globals_address = globals_gpu_addr;
        header.instances_address = instances_addr;
        header.texture_id = texture_id;
        header.instance_count = batch.instance_count;
        header.vbo_address = buffer->get_gpu_address();
        if (!header.vbo_address) continue;

        if (auto uv1 = primitive->get_uv1_buffer()) {
            uint64_t uv1_base = uv1->get_gpu_address();
            if (!uv1_base) continue;
            header.uv1_address = uv1_base + primitive->get_uv1_offset();
            header.uv1_enabled = 1;
        } else {
            auto def = get_default_buffer(DefaultBufferType::Uv1);
            header.uv1_address = def ? def->get_gpu_address() : 0;
            header.uv1_enabled = 0;
            if (!header.uv1_address) continue;
        }

        uint64_t material_addr = detail::write_material_once(
            batch.material.get(), frame_data,
            static_cast<ITextureResolver*>(&resources),
            material_cache);

        constexpr size_t kMaterialPtrSize = sizeof(uint64_t);
        size_t total_size = sizeof(DrawDataHeader) + kMaterialPtrSize;

        auto reservation = frame_data.reserve(total_size);
        if (!reservation.ptr) {
            continue;
        }
        auto* dst = static_cast<uint8_t*>(reservation.ptr);
        uint64_t draw_data_addr = reservation.gpu_addr;
        std::memcpy(dst, &header, sizeof(header));
        std::memcpy(dst + sizeof(DrawDataHeader), &material_addr, kMaterialPtrSize);

        // Resolve the G-buffer pipeline variant. Base key is the forward
        // pipeline key; perturb by the visual's discard-snippet class
        // so two visuals sharing a material get distinct gbuffer
        // pipelines with the right `velk_visual_discard` spliced in.
        uint64_t forward_key = batch.pipeline_key;
        if (forward_key == 0 && batch.material) {
            forward_key = batch.material->get_pipeline_handle(*this);
        }
        if (forward_key == 0) {
            continue;
        }
        uint64_t gbuffer_key = forward_key ^ batch.discard_key_perturb;

        PipelineId gpid = 0;
        auto git = gbuffer_pipeline_map_.find(gbuffer_key);
        if (git != gbuffer_pipeline_map_.end()) {
            gpid = git->second;
        } else {
            string_view vsrc;
            string_view base_fsrc;
            string composed_fsrc;

            if (batch.material) {
                if (auto* mat = interface_cast<IMaterial>(batch.material.get());
                    mat && !mat->get_eval_src().empty()
                    && !mat->get_vertex_src().empty()
                    && !mat->get_eval_fn_name().empty()) {
                    mat->register_eval_includes(*this);
                    composed_fsrc = compose_eval_fragment(
                        deferred_fragment_driver_template,
                        mat->get_eval_src(),
                        mat->get_eval_fn_name(),
                        mat->get_deferred_discard_threshold());
                    vsrc = mat->get_vertex_src();
                    base_fsrc = string_view(composed_fsrc);
                }
            }
            if (base_fsrc.empty()) {
                base_fsrc = default_gbuffer_fragment_src;
            }
            if (vsrc.empty() && batch.raster_shader) {
                auto rsrc = batch.raster_shader->get_raster_source(
                    IRasterShader::Target::Forward);
                if (!rsrc.vertex.empty()) {
                    vsrc = rsrc.vertex;
                }
            }

            string composed;
            composed.append(base_fsrc);
            composed.append(string_view("\n", 1));
            if (batch.visual_discard) {
                composed.append(batch.visual_discard->get_snippet_source());
            } else {
                composed.append(string_view("void velk_visual_discard() {}\n", 30));
            }

            auto po = pipeline_options_from_storage(
                interface_cast<IObjectStorage>(batch.material.get()));
            po.topology = to_backend_topology(primitive->get_topology());
            gpid = compile_gbuffer_pipeline(
                string_view(composed), vsrc, gbuffer_key, target_group, po);
        }
        if (gpid == 0) {
            continue;
        }

        (void)observer; // reserved for future per-resource registration

        DrawCall call{};
        call.pipeline = gpid;
        if (ibo_handle) {
            call.index_buffer = ibo_handle;
            call.index_buffer_offset = ibo_offset;
            call.index_count = primitive->get_index_count();
        } else {
            call.vertex_count = primitive->get_vertex_count();
        }
        call.instance_count = batch.instance_count;
        call.root_constants_size = sizeof(uint64_t);
        std::memcpy(call.root_constants, &draw_data_addr, sizeof(uint64_t));

        out_calls.push_back(call);
    }

    return out_calls;
}

} // namespace velk
