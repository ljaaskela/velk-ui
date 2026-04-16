#include "renderer.h"

#include "default_ui_shaders.h"

#include <velk/api/any.h>
#include <velk/api/perf.h>
#include <velk/api/state.h>
#include <velk/api/velk.h>
#include <velk/interface/intf_object_storage.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <velk-render/interface/intf_material.h>
#include <velk-ui/interface/intf_camera.h>
#include <velk-ui/interface/intf_environment.h>
#include <velk-ui/interface/intf_visual.h>

#ifdef VELK_RENDER_DEBUG
#define RENDER_LOG(...) VELK_LOG(I, __VA_ARGS__)
#else
#define RENDER_LOG(...) ((void)0)
#endif

namespace velk::ui {

namespace {

void build_ortho_projection(float* out, float width, float height)
{
    std::memset(out, 0, 16 * sizeof(float));
    out[0] = 2.0f / width;
    out[5] = 2.0f / height;
    out[10] = -1.0f;
    out[12] = -1.0f;
    out[13] = -1.0f;
    out[15] = 1.0f;
}

} // namespace

constexpr string_view velk_ui_glsl = R"(
struct RectInstance {
    vec2 pos;
    vec2 size;
    vec4 color;
};

struct TextInstance {
    vec2 pos;
    vec2 size;
    vec4 color;
    uint glyph_index;
    uint _pad0;
    uint _pad1;
    uint _pad2;
};

layout(buffer_reference, std430) readonly buffer RectInstanceData {
    RectInstance data[];
};

layout(buffer_reference, std430) readonly buffer TextInstanceData {
    TextInstance data[];
};
)";

void Renderer::set_backend(const IRenderBackend::Ptr& backend, IRenderContext* ctx)
{
    backend_ = backend;
    render_ctx_ = ctx;

    if (!backend_) {
        return;
    }

    pipeline_map_ = &ctx->pipeline_map();

    // Register velk-ui shader include for UI instance types

    ctx->register_shader_include("velk-ui.glsl", velk_ui_glsl);

    // Register default shaders. Visuals and materials that do not provide
    // their own shader sources fall back to these (see compile_pipeline:
    // empty source -> registered default). Built-in UI pipelines are now
    // compiled lazily by batch_builder on first sight of a new visual type.
    ctx->set_default_vertex_shader(ctx->compile_shader(default_vertex_src, ShaderStage::Vertex));
    ctx->set_default_fragment_shader(ctx->compile_shader(default_fragment_src, ShaderStage::Fragment));

    frame_buffer_.init();
    for (auto& slot : frame_slots_) {
        frame_buffer_.init_slot(slot.buffer, *backend_);
    }
}

void Renderer::add_view(const IElement::Ptr& camera_element, const IWindowSurface::Ptr& surface,
                        const rect& viewport)
{
    if (!(camera_element && surface)) {
        VELK_LOG(E, "Renderer::add_view: camera_element and surface must be valid");
        return;
    }

    // Create the backend surface if this is the first view using it
    if (get_render_target_id(surface) == 0 && backend_) {
        auto state = read_state<IWindowSurface>(surface);
        if (state) {
            SurfaceDesc desc{};
            desc.width = state->size.x;
            desc.height = state->size.y;
            desc.update_rate = state->update_rate;
            desc.target_fps = state->target_fps;
            uint64_t sid = backend_->create_surface(desc);
            if (auto* rt = interface_cast<IRenderTarget>(surface)) {
                rt->set_render_target_id(sid);
            }
        }
    }
    views_.push_back({camera_element, surface, viewport});
}

void Renderer::remove_view(const IElement::Ptr& camera_element, const IWindowSurface::Ptr& surface)
{
    for (auto it = views_.begin(); it != views_.end(); ++it) {
        if (it->camera_element == camera_element && it->surface == surface) {
            if (backend_) {
                if (it->rt_output_tex != 0) {
                    resources_.defer_texture_destroy(
                        it->rt_output_tex, present_counter_ + kGpuLatencyFrames);
                }
                backend_->destroy_surface(get_render_target_id(it->surface));
            }
            views_.erase(it);
            return;
        }
    }
}

bool Renderer::view_matches(const ViewEntry& entry, const FrameDesc& desc) const
{
    if (desc.views.empty()) {
        return true;
    }
    for (auto& vd : desc.views) {
        if (vd.surface != entry.surface) {
            continue;
        }
        if (vd.cameras.empty()) {
            return true;
        }
        for (auto& cam : vd.cameras) {
            if (cam == entry.camera_element) {
                return true;
            }
        }
    }
    return false;
}

Renderer::FrameSlot* Renderer::claim_frame_slot()
{
    auto is_slot_free = [&](const FrameSlot& s) {
        return !s.ready && (s.presented_at == 0 || present_counter_ - s.presented_at >= kGpuLatencyFrames);
    };

    FrameSlot* slot = nullptr;
    {
        std::unique_lock<std::mutex> lock(slot_mutex_);
        slot_cv_.wait(lock, [&] {
            for (auto& s : frame_slots_) {
                if (is_slot_free(s)) {
                    return true;
                }
            }
            return false;
        });
        for (auto& s : frame_slots_) {
            if (is_slot_free(s)) {
                slot = &s;
                break;
            }
        }
    }

    slot->id = next_frame_id_++;
    slot->passes.clear();
    return slot;
}

std::unordered_map<IScene*, SceneState> Renderer::consume_scenes(const FrameDesc& desc)
{
    std::unordered_map<IScene*, SceneState> consumed;
    for (auto& entry : views_) {
        if (!view_matches(entry, desc)) {
            continue;
        }

        auto scene_ptr = entry.camera_element->get_scene();
        auto* scene = interface_cast<IScene>(scene_ptr);
        if (!scene || consumed.count(scene)) {
            continue;
        }

        // Handle surface resize
        auto sstate = read_state<IWindowSurface>(entry.surface);
        if (sstate) {
            if (sstate->size.x != entry.cached_width || sstate->size.y != entry.cached_height) {
                entry.cached_width = sstate->size.x;
                entry.cached_height = sstate->size.y;
                backend_->resize_surface(get_render_target_id(entry.surface), sstate->size.x, sstate->size.y);
                entry.batches_dirty = true;
                RENDER_LOG("render: surface resized to %dx%d", sstate->size.x, sstate->size.y);
            }
        }

        auto state = scene->consume_state();
        bool has_changes = !state.redraw_list.empty() || !state.removed_list.empty();

        // Evict removed elements
        for (auto& removed : state.removed_list) {
            batch_builder_.evict(removed.get());
            auto rit = render_target_entries_.find(removed.get());
            if (rit != render_target_entries_.end()) {
                if (rit->second.texture_id != 0 && backend_) {
                    resources_.defer_texture_destroy(rit->second.texture_id,
                                                     present_counter_ + kGpuLatencyFrames);
                }
                render_target_entries_.erase(rit);
            }
        }

        // Rebuild draw commands for changed elements
        for (auto* element : state.redraw_list) {
            batch_builder_.rebuild_commands(element, this, render_ctx_);
        }

        // Upload dirty GPU resources
        bool resources_uploaded = false;
        if (has_changes) {
            for (auto& [elem, cache] : batch_builder_.element_cache()) {
                for (auto& weak : cache.gpu_resources) {
                    auto buf_ptr = weak.lock();
                    auto* buf = buf_ptr.get();
                    if (!buf || !buf->is_dirty()) {
                        continue;
                    }

                    if (auto* surf = interface_cast<ISurface>(buf)) {
                        auto sz = surf->get_dimensions();
                        int tw = static_cast<int>(sz.x);
                        int th = static_cast<int>(sz.y);
                        const uint8_t* pixels = buf->get_data();
                        if (pixels && tw > 0 && th > 0) {
                            TextureId tid = resources_.find_texture(surf);
                            if (tid == 0) {
                                TextureDesc tdesc{};
                                tdesc.width = tw;
                                tdesc.height = th;
                                tdesc.format = surf->format();
                                tid = backend_->create_texture(tdesc);
                                resources_.register_texture(surf, tid);
                            }
                            backend_->upload_texture(tid, pixels, tw, th);
                            buf->clear_dirty();
                            resources_uploaded = true;
                        }
                    } else {
                        size_t bsize = buf->get_data_size();
                        const uint8_t* bytes = buf->get_data();
                        if (!bytes || bsize == 0) {
                            continue;
                        }
                        auto* be = resources_.find_buffer(buf);
                        bool need_alloc = (be == nullptr);
                        if (!need_alloc && be->size != bsize) {
                            resources_.defer_buffer_destroy(be->handle, present_counter_ + kGpuLatencyFrames);
                            resources_.unregister_buffer(buf);
                            be = nullptr;
                            need_alloc = true;
                        }
                        if (need_alloc) {
                            GpuBufferDesc bdesc{};
                            bdesc.size = bsize;
                            bdesc.cpu_writable = true;
                            GpuResourceManager::BufferEntry bentry{};
                            bentry.handle = backend_->create_buffer(bdesc);
                            bentry.size = bsize;
                            resources_.register_buffer(buf, bentry);
                            be = resources_.find_buffer(buf);
                            buf->set_gpu_address(backend_->gpu_address(bentry.handle));
                        }
                        if (auto* dst = backend_->map(be->handle)) {
                            std::memcpy(dst, bytes, bsize);
                        }
                        buf->clear_dirty();
                        resources_uploaded = true;
                    }
                }
            }
        }

        if (has_changes || resources_uploaded) {
            for (auto& v : views_) {
                auto sp = v.camera_element->get_scene();
                if (interface_cast<IScene>(sp) == scene) {
                    v.batches_dirty = true;
                }
            }
        }

        consumed[scene] = std::move(state);
    }
    return consumed;
}

void Renderer::build_frame_passes(const FrameDesc& desc,
                                  std::unordered_map<IScene*, SceneState>& consumed_scenes, FrameSlot& slot)
{
    static constexpr int kMaxRecordRetries = 3;
    for (int attempt = 0;; ++attempt) {
        frame_buffer_.begin_frame(slot.buffer);
        slot.passes.clear();

        for (auto& entry : views_) {
            if (!view_matches(entry, desc)) {
                continue;
            }

            auto scene_ptr = entry.camera_element->get_scene();
            auto* scene = interface_cast<IScene>(scene_ptr);
            if (!scene) {
                continue;
            }

            auto sit = consumed_scenes.find(scene);
            if (sit == consumed_scenes.end()) {
                continue;
            }

            ICamera* camera = nullptr;
            if (auto* storage = interface_cast<IObjectStorage>(entry.camera_element)) {
                camera = interface_cast<ICamera>(storage->find_attachment<ICamera>());
            }

            // Ray-traced view: skip raster batch work, build a compute+blit
            // pass targeting the surface. The camera's render_path drives
            // this; raster is the default when no camera or path=Raster.
            RenderPath render_path = RenderPath::Raster;
            if (camera) {
                auto cam_state = read_state<ICamera>(camera);
                if (cam_state) {
                    render_path = cam_state->render_path;
                }
            }

            if (render_path == RenderPath::RayTrace) {
                auto sstate_rt = read_state<IWindowSurface>(entry.surface);
                float sw_full = static_cast<float>(sstate_rt ? sstate_rt->size.x : 0);
                float sh_full = static_cast<float>(sstate_rt ? sstate_rt->size.y : 0);
                bool has_vp = entry.viewport.width > 0 && entry.viewport.height > 0;
                float vp_x_f = has_vp ? entry.viewport.x * sw_full : 0.f;
                float vp_y_f = has_vp ? entry.viewport.y * sh_full : 0.f;
                float vp_w_f = has_vp ? entry.viewport.width * sw_full : sw_full;
                float vp_h_f = has_vp ? entry.viewport.height * sh_full : sh_full;
                int vp_w = static_cast<int>(vp_w_f);
                int vp_h = static_cast<int>(vp_h_f);
                if (vp_w <= 0 || vp_h <= 0 || !backend_ || !render_ctx_) {
                    continue;
                }

                if (entry.rt_output_tex != 0 &&
                    (entry.rt_width != vp_w || entry.rt_height != vp_h)) {
                    resources_.defer_texture_destroy(
                        entry.rt_output_tex, present_counter_ + kGpuLatencyFrames);
                    entry.rt_output_tex = 0;
                }
                if (entry.rt_output_tex == 0) {
                    TextureDesc td{};
                    td.width = vp_w;
                    td.height = vp_h;
                    td.format = PixelFormat::RGBA8;
                    td.usage = TextureUsage::Storage;
                    entry.rt_output_tex = backend_->create_texture(td);
                    entry.rt_width = vp_w;
                    entry.rt_height = vp_h;
                }
                if (entry.rt_output_tex == 0) {
                    continue;
                }

                // Camera view-projection + world position for ray generation.
                mat4 vp_mat;
                if (camera) {
                    vp_mat = camera->get_view_projection(*entry.camera_element,
                                                         static_cast<float>(vp_w),
                                                         static_cast<float>(vp_h));
                } else {
                    float tmp[16];
                    build_ortho_projection(tmp, static_cast<float>(vp_w),
                                           static_cast<float>(vp_h));
                    std::memcpy(vp_mat.m, tmp, sizeof(vp_mat.m));
                }
                auto inv_vp = mat4::inverse(vp_mat);
                float cam_px = 0.f, cam_py = 0.f, cam_pz = 0.f;
                if (auto es = read_state<IElement>(entry.camera_element)) {
                    cam_px = es->world_matrix(0, 3);
                    cam_py = es->world_matrix(1, 3);
                    cam_pz = es->world_matrix(2, 3);
                }

                struct RtShape {
                    float origin[4];    // xyz world origin; w reserved
                    float u_axis[4];    // xyz = local x dir * width; w reserved
                    float v_axis[4];    // xyz = local y dir * height; w reserved
                    float color[4];
                    float params[4];    // x = corner radius (world units)
                    uint32_t material_id;
                    uint32_t texture_id;
                    uint32_t shape_param;
                    uint32_t _pad;
                    uint64_t material_data_addr;
                    uint64_t _tail_pad;
                };
                static_assert(sizeof(RtShape) == 112, "RtShape layout mismatch");

                auto register_material = [&](IProgram* prog) -> uint32_t {
                    if (!prog) return 0;
                    auto fill = prog->get_fill_src();
                    auto fn = prog->get_fill_fn_name();
                    auto inc = prog->get_fill_include_name();
                    if (fill.empty() || fn.empty() || inc.empty()) return 0;
                    auto* obj = interface_cast<IObject>(prog);
                    if (!obj) return 0;
                    Uid uid = obj->get_class_uid();
                    uint64_t key = uid.hi ^ uid.lo;
                    auto it = rt_material_id_by_class_.find(key);
                    if (it != rt_material_id_by_class_.end()) {
                        return it->second;
                    }
                    // Register the material's fill snippet as a shader
                    // include so the composed shader can `#include "<name>"`
                    // and get proper line-number diagnostics in compile errors.
                    render_ctx_->register_shader_include(inc, fill);
                    // Let the material register any further include
                    // dependencies (e.g. the text material's velk_text.glsl).
                    prog->register_fill_includes(*render_ctx_);
                    uint32_t id = static_cast<uint32_t>(rt_material_info_by_id_.size()) + 1;
                    rt_material_info_by_id_.push_back({fn, inc});
                    rt_material_id_by_class_[key] = id;
                    return id;
                };

                vector<RtShape> shapes;
                shapes.reserve(sit->second.visual_list.size());
                rt_frame_materials_.clear();

                for (auto& ve : sit->second.visual_list) {
                    if (ve.type != VisualEntry::Element || !ve.element) continue;
                    auto es = read_state<IElement>(ve.element);
                    if (!es) continue;
                    auto* storage = interface_cast<IObjectStorage>(ve.element);
                    if (!storage) continue;
                    // Element world origin and local x/y axis basis. TRS/orbit
                    // etc. compose into world_matrix; for a flat UI panel the
                    // z components stay zero and we get the 2D behaviour back.
                    float ox = es->world_matrix(0, 3);
                    float oy = es->world_matrix(1, 3);
                    float oz = es->world_matrix(2, 3);
                    float ux = es->world_matrix(0, 0);
                    float uy = es->world_matrix(1, 0);
                    float uz = es->world_matrix(2, 0);
                    float vx = es->world_matrix(0, 1);
                    float vy = es->world_matrix(1, 1);
                    float vz = es->world_matrix(2, 1);
                    float ew = es->size.width;
                    float eh = es->size.height;
                    for (size_t j = 0; j < storage->attachment_count(); ++j) {
                        auto* visual = interface_cast<IVisual>(storage->get_attachment(j));
                        if (!visual) continue;
                        auto vs = read_state<IVisual>(visual);
                        if (!vs) continue;

                        // Upload the material's per-draw data once per visual
                        // (shared across all this visual's draw entries).
                        uint32_t mat_id = 0;
                        uint64_t mat_addr = 0;
                        if (vs->paint) {
                            auto prog_ptr = vs->paint.get<IProgram>();
                            IProgram* prog = prog_ptr.get();
                            mat_id = register_material(prog);
                            if (mat_id == 0) {
                                // Paint present but material has no RT fill
                                // snippet; skip this visual entirely rather
                                // than flood the bounds with its base color.
                                continue;
                            }
                            size_t sz = prog->gpu_data_size();
                            if (sz > 0) {
                                void* scratch = std::malloc(sz);
                                if (scratch) {
                                    std::memset(scratch, 0, sz);
                                    if (prog->write_gpu_data(scratch, sz) == ReturnValue::Success) {
                                        mat_addr = frame_buffer_.write(scratch, sz);
                                    }
                                    std::free(scratch);
                                }
                            }
                            bool seen = false;
                            for (auto id : rt_frame_materials_) {
                                if (id == mat_id) { seen = true; break; }
                            }
                            if (!seen) rt_frame_materials_.push_back(mat_id);
                        }

                        float radius = 0.f;
                        if (!visual->get_intersect_src().empty()) {
                            radius = std::min(std::min(ew, eh) * 0.5f, 12.f);
                        }

                        // Iterate the visual's draw entries; one RT shape per
                        // entry. Each entry's instance_data carries
                        // pos/size/color (standard prefix) and optionally
                        // shape_param at offset 32 (e.g. glyph_index for text).
                        rect local_rect{0, 0, ew, eh};
                        auto entries = visual->get_draw_entries(local_rect);
                        for (auto& entry : entries) {
                            if (entry.instance_size < 16) {
                                continue; // no pos/size, nothing to draw
                            }
                            float local_px = 0, local_py = 0, sz_w = 0, sz_h = 0;
                            std::memcpy(&local_px, entry.instance_data + 0, 4);
                            std::memcpy(&local_py, entry.instance_data + 4, 4);
                            std::memcpy(&sz_w, entry.instance_data + 8, 4);
                            std::memcpy(&sz_h, entry.instance_data + 12, 4);

                            float cr = 0, cg = 0, cb = 0, ca = 1;
                            if (entry.instance_size >= 32) {
                                std::memcpy(&cr, entry.instance_data + 16, 4);
                                std::memcpy(&cg, entry.instance_data + 20, 4);
                                std::memcpy(&cb, entry.instance_data + 24, 4);
                                std::memcpy(&ca, entry.instance_data + 28, 4);
                            } else {
                                cr = vs->color.r; cg = vs->color.g;
                                cb = vs->color.b; ca = vs->color.a;
                            }

                            uint32_t shape_param = 0;
                            if (entry.instance_size >= 36) {
                                std::memcpy(&shape_param, entry.instance_data + 32, 4);
                            }

                            uint32_t tex_id = 0;
                            if (entry.texture_key != 0) {
                                auto* surf = reinterpret_cast<ISurface*>(
                                    static_cast<uintptr_t>(entry.texture_key));
                                tex_id = resources_.find_texture(surf);
                            }

                            // Shape origin in world space = element origin +
                            // local (px, py) offset along element's axes.
                            float sx = ox + ux * local_px + vx * local_py;
                            float sy = oy + uy * local_px + vy * local_py;
                            float sz = oz + uz * local_px + vz * local_py;

                            RtShape s{};
                            s.origin[0] = sx;
                            s.origin[1] = sy;
                            s.origin[2] = sz;
                            s.origin[3] = 0.f;
                            s.u_axis[0] = ux * sz_w;
                            s.u_axis[1] = uy * sz_w;
                            s.u_axis[2] = uz * sz_w;
                            s.u_axis[3] = 0.f;
                            s.v_axis[0] = vx * sz_h;
                            s.v_axis[1] = vy * sz_h;
                            s.v_axis[2] = vz * sz_h;
                            s.v_axis[3] = 0.f;
                            s.color[0] = cr;
                            s.color[1] = cg;
                            s.color[2] = cb;
                            s.color[3] = ca;
                            s.params[0] = radius;
                            s.material_id = mat_id;
                            s.texture_id = tex_id;
                            s.shape_param = shape_param;
                            s.material_data_addr = mat_addr;
                            shapes.push_back(s);
                        }
                    }
                }

                // Environment: if the camera has one, ensure its texture is
                // uploaded and its material is registered BEFORE compiling the
                // RT pipeline (so env's fill snippet is composed in).
                uint32_t env_mat_id = 0;
                uint32_t env_tex_id = 0;
                uint64_t env_data_addr = 0;
                if (camera) {
                    // Reuses the raster path's env-texture-upload logic. The
                    // synthetic batch it appends to entry.batches is ignored
                    // on the RT path (we skip batch processing).
                    prepend_environment_batch(*camera, entry);

                    auto cam_state2 = read_state<ICamera>(camera);
                    if (cam_state2 && cam_state2->environment) {
                        auto env_ptr = cam_state2->environment.get<IEnvironment>();
                        if (env_ptr) {
                            if (auto* env_surf = interface_cast<ISurface>(env_ptr)) {
                                env_tex_id = resources_.find_texture(env_surf);
                            }
                            auto env_mat = env_ptr->get_material();
                            if (env_mat) {
                                auto env_prog = interface_pointer_cast<IProgram>(env_mat);
                                IProgram* prog = env_prog.get();
                                env_mat_id = register_material(prog);
                                if (env_mat_id != 0) {
                                    size_t sz = prog->gpu_data_size();
                                    if (sz > 0) {
                                        void* scratch = std::malloc(sz);
                                        if (scratch) {
                                            std::memset(scratch, 0, sz);
                                            if (prog->write_gpu_data(scratch, sz) == ReturnValue::Success) {
                                                env_data_addr = frame_buffer_.write(scratch, sz);
                                            }
                                            std::free(scratch);
                                        }
                                    }
                                    bool seen = false;
                                    for (auto id : rt_frame_materials_) {
                                        if (id == env_mat_id) { seen = true; break; }
                                    }
                                    if (!seen) rt_frame_materials_.push_back(env_mat_id);
                                }
                            }
                        }
                    }
                }

                // Sort for deterministic hashing regardless of encounter order.
                std::sort(rt_frame_materials_.begin(), rt_frame_materials_.end());
                uint64_t rt_pipeline_key = ensure_rt_pipeline(rt_frame_materials_);
                if (rt_pipeline_key == 0) {
                    continue;
                }
                auto pit = pipeline_map_->find(rt_pipeline_key);
                if (pit == pipeline_map_->end()) {
                    continue;
                }

                // Compositing order is scene enumeration order (DFS/JSON).
                // A camera-distance depth sort breaks that for a flat UI
                // viewed at an angle, because coplanar shapes at different
                // XY end up at different camera-space depths. Proper depth
                // sorting becomes necessary once shapes live at genuinely
                // different world z; until then, enumeration is authoritative.

                uint64_t shapes_addr = 0;
                if (!shapes.empty()) {
                    shapes_addr = frame_buffer_.write(
                        shapes.data(), shapes.size() * sizeof(RtShape));
                }

                struct PushC {
                    float inv_vp[16];
                    float cam_pos[4];
                    uint32_t image_index;
                    uint32_t width;
                    uint32_t height;
                    uint32_t shape_count;
                    uint32_t env_material_id;
                    uint32_t env_texture_id;
                    uint32_t _env_pad0;
                    uint32_t _env_pad1;
                    uint64_t shapes_addr;
                    uint64_t env_data_addr;
                };
                static_assert(sizeof(PushC) == 128, "PushC layout mismatch");

                PushC pc{};
                std::memcpy(pc.inv_vp, inv_vp.m, sizeof(pc.inv_vp));
                pc.cam_pos[0] = cam_px;
                pc.cam_pos[1] = cam_py;
                pc.cam_pos[2] = cam_pz;
                pc.cam_pos[3] = 0.f;
                pc.image_index = entry.rt_output_tex;
                pc.width = static_cast<uint32_t>(vp_w);
                pc.height = static_cast<uint32_t>(vp_h);
                pc.shape_count = static_cast<uint32_t>(shapes.size());
                pc.env_material_id = env_mat_id;
                pc.env_texture_id = env_tex_id;
                pc.shapes_addr = shapes_addr;
                pc.env_data_addr = env_data_addr;

                RenderPass pass;
                pass.kind = PassKind::ComputeBlit;
                pass.compute.pipeline = pit->second;
                pass.compute.groups_x = (vp_w + 7) / 8;
                pass.compute.groups_y = (vp_h + 7) / 8;
                pass.compute.groups_z = 1;
                pass.compute.root_constants_size = sizeof(PushC);
                std::memcpy(pass.compute.root_constants, &pc, sizeof(PushC));
                pass.blit_source = entry.rt_output_tex;
                pass.blit_surface_id = get_render_target_id(entry.surface);
                pass.blit_dst_rect = {vp_x_f, vp_y_f, vp_w_f, vp_h_f};
                slot.passes.push_back(std::move(pass));
                continue;
            }

            if (entry.batches_dirty) {
                batch_builder_.rebuild_batches(sit->second, entry.batches);
                if (camera) {
                    prepend_environment_batch(*camera, entry);
                }
                entry.batches_dirty = false;
            }

            auto sstate = read_state<IWindowSurface>(entry.surface);
            float sw = static_cast<float>(sstate ? sstate->size.x : 0);
            float sh = static_cast<float>(sstate ? sstate->size.y : 0);
            bool has_viewport = entry.viewport.width > 0 && entry.viewport.height > 0;
            float vp_w = has_viewport ? entry.viewport.width * sw : sw;
            float vp_h = has_viewport ? entry.viewport.height * sh : sh;

            if (vp_w > 0 && vp_h > 0) {
                FrameGlobals globals{};
                mat4 vp_mat;
                if (camera) {
                    vp_mat = camera->get_view_projection(*entry.camera_element, vp_w, vp_h);
                } else {
                    build_ortho_projection(globals.view_projection, vp_w, vp_h);
                    std::memcpy(vp_mat.m, globals.view_projection, sizeof(vp_mat.m));
                }
                std::memcpy(globals.view_projection, vp_mat.m, sizeof(vp_mat.m));
                auto inv_vp = mat4::inverse(vp_mat);
                std::memcpy(globals.inverse_view_projection, inv_vp.m, sizeof(inv_vp.m));
                globals.viewport[0] = vp_w;
                globals.viewport[1] = vp_h;
                globals.viewport[2] = 1.0f / vp_w;
                globals.viewport[3] = 1.0f / vp_h;
                globals_gpu_addr_ = frame_buffer_.write(&globals, sizeof(globals));
            }

            draw_calls_.clear();
            batch_builder_.build_draw_calls(entry.batches,
                                            draw_calls_,
                                            frame_buffer_,
                                            resources_,
                                            globals_gpu_addr_,
                                            pipeline_map_,
                                            render_ctx_,
                                            this);

            // Main surface pass
            RenderPass pass;
            pass.target.target = interface_pointer_cast<IRenderTarget>(entry.surface);
            float vp_x = has_viewport ? entry.viewport.x * sw : 0;
            float vp_y = has_viewport ? entry.viewport.y * sh : 0;
            pass.viewport = {vp_x, vp_y, vp_w, vp_h};
            pass.draw_calls = draw_calls_;
            slot.passes.push_back(std::move(pass));
        }

        // Build render target passes once (not per view)
        for (auto& rtp : batch_builder_.render_target_passes()) {
            auto& rte = render_target_entries_[rtp.element];
            if (!rte.target) {
                if (auto* storage = interface_cast<IObjectStorage>(rtp.element)) {
                    if (auto rtt = storage->find_attachment<IRenderToTexture>()) {
                        auto rtt_state = read_state<IRenderToTexture>(rtt);
                        if (rtt_state) {
                            rte.target = rtt_state->render_target.get<IRenderTarget>();
                        }
                    }
                }
            }
            if (!rte.target) {
                continue;
            }
            int w{1}, h{1};
            if (auto es = read_state<IElement>(rtp.element)) {
                w = std::max(static_cast<int>(es->size.width), 1);
                h = std::max(static_cast<int>(es->size.height), 1);
            }
            if (rte.texture_id != 0 && (rte.width != w || rte.height != h)) {
                if (backend_) {
                    resources_.defer_texture_destroy(rte.texture_id, present_counter_ + kGpuLatencyFrames);
                }
                rte.texture_id = 0;
            }
            if (rte.texture_id == 0 && backend_) {
                TextureDesc tdesc{};
                tdesc.width = w;
                tdesc.height = h;
                tdesc.format = PixelFormat::RGBA8;
                tdesc.usage = TextureUsage::RenderTarget;
                rte.texture_id = backend_->create_texture(tdesc);
                rte.width = w;
                rte.height = h;
                rte.target->set_render_target_id(static_cast<uint64_t>(rte.texture_id));
            }

            if (rte.texture_id == 0) {
                continue;
            }

            FrameGlobals rt_globals{};
            build_ortho_projection(
                rt_globals.view_projection, static_cast<float>(rte.width), static_cast<float>(rte.height));
            rt_globals.viewport[0] = static_cast<float>(rte.width);
            rt_globals.viewport[1] = static_cast<float>(rte.height);
            rt_globals.viewport[2] = 1.0f / static_cast<float>(rte.width);
            rt_globals.viewport[3] = 1.0f / static_cast<float>(rte.height);
            uint64_t saved_globals = globals_gpu_addr_;
            globals_gpu_addr_ = frame_buffer_.write(&rt_globals, sizeof(rt_globals));

            vector<DrawCall> saved_calls;
            std::swap(draw_calls_, saved_calls);
            batch_builder_.build_draw_calls(rtp.batches,
                                            draw_calls_,
                                            frame_buffer_,
                                            resources_,
                                            globals_gpu_addr_,
                                            pipeline_map_,
                                            render_ctx_,
                                            this);

            // Insert render target pass before surface passes
            RenderPass rt_pass{};
            rt_pass.target.target = rte.target;
            rt_pass.viewport = {0, 0, static_cast<float>(rte.width), static_cast<float>(rte.height)};
            rt_pass.draw_calls = draw_calls_;
            slot.passes.insert(slot.passes.begin(), std::move(rt_pass));

            std::swap(draw_calls_, saved_calls);
            globals_gpu_addr_ = saved_globals;
            rte.dirty = false;
        }

        if (!frame_buffer_.overflowed()) {
            break;
        }
        if (attempt >= kMaxRecordRetries) {
            VELK_LOG(
                E, "Renderer: frame buffer overflow after %d retries, dropping frame", kMaxRecordRetries);
            break;
        }
        frame_buffer_.grow(*backend_);
    } // retry loop
}

Frame Renderer::prepare(const FrameDesc& desc)
{
    if (!backend_) {
        return {};
    }
    VELK_PERF_EVENT(Render);
    VELK_PERF_SCOPE("renderer.prepare");

    // Drain any GPU resources whose safe window has elapsed.
    resources_.drain_deferred(*backend_, present_counter_);

    auto* slot = claim_frame_slot();
    active_slot_ = slot;

    // If this slot's buffer is undersized (it was in-flight during a
    // previous regrow and was skipped), grow it now that it's safe.
    frame_buffer_.ensure_slot(slot->buffer, *backend_);

    // Prepare the staging buffer once for all views
    frame_buffer_.ensure_capacity(*backend_);

    if ((slot->id % 10000) == 0) {
        VELK_LOG(I,
                 "Renderer: frame %llu, frame buffer %zu KB, peak usage %zu KB",
                 static_cast<unsigned long long>(slot->id),
                 frame_buffer_.get_buffer_size() / 1024,
                 frame_buffer_.get_peak_usage() / 1024);
    }

    auto consumed_scenes = consume_scenes(desc);

    build_frame_passes(desc, consumed_scenes, *slot);

    active_slot_ = nullptr;
    slot->ready = true;
    return Frame{slot->id};
}

void Renderer::present(Frame frame)
{
    if (!backend_ || frame.id == 0) {
        VELK_PERF_EVENT(Present);
        return;
    }
    {
        VELK_PERF_SCOPE("renderer.present");

        std::lock_guard<std::mutex> lock(slot_mutex_);

        // Find the frame being presented to determine which surfaces it targets
        FrameSlot* target = nullptr;
        for (auto& s : frame_slots_) {
            if (s.ready && s.id == frame.id) {
                target = &s;
                break;
            }
        }
        if (!target) {
            slot_cv_.notify_one();
            return;
        }

        // Discard older unpresented frames that target overlapping surfaces
        for (auto& s : frame_slots_) {
            if (!s.ready || s.id >= frame.id) {
                continue;
            }
            // Remove passes that overlap with the frame being presented
            for (auto it = s.passes.begin(); it != s.passes.end();) {
                bool overlaps = false;
                uint64_t it_id = get_render_target_id(it->target.target);
                for (auto& tp : target->passes) {
                    uint64_t tp_id = get_render_target_id(tp.target.target);
                    if (it_id == tp_id) {
                        overlaps = true;
                        break;
                    }
                }
                if (overlaps) {
                    it = s.passes.erase(it);
                } else {
                    ++it;
                }
            }
            // If all passes were removed, mark slot as not ready but preserve presented_at
            // so the buffer isn't reused before the GPU is done with it
            if (s.passes.empty()) {
                s.ready = false;
                s.presented_at = present_counter_ + 1;
            }
        }

        // Submit all passes within a single frame
        {
            VELK_PERF_SCOPE("renderer.wait_vsync");
            backend_->begin_frame();
        }
        bool had_texture_pass = false;
        for (size_t i = 0; i < target->passes.size(); ++i) {
            auto& pass = target->passes[i];

            if (pass.kind == PassKind::ComputeBlit) {
                // Ray-traced view: dispatch compute then blit output texture
                // onto the swapchain image for the surface.
                if (had_texture_pass) {
                    backend_->barrier(PipelineStage::ColorOutput, PipelineStage::FragmentShader);
                    had_texture_pass = false;
                }
                backend_->dispatch({&pass.compute, 1});
                backend_->blit_to_surface(pass.blit_source, pass.blit_surface_id, pass.blit_dst_rect);
                continue;
            }

            auto* rt = pass.target.target.get();
            uint64_t pass_target_id = rt ? rt->get_render_target_id() : 0;
            bool is_texture = rt && rt->get_type() == GpuResourceType::Texture;

            // Insert barrier when transitioning from texture passes to surface pass
            if (!is_texture && had_texture_pass) {
                backend_->barrier(PipelineStage::ColorOutput, PipelineStage::FragmentShader);
                had_texture_pass = false;
            }

            backend_->begin_pass(pass_target_id);

            RENDER_LOG(
                "present: submitting %zu draw calls to target %llu", pass.draw_calls.size(), pass_target_id);
            {
                VELK_PERF_SCOPE("renderer.submit");
                backend_->submit({pass.draw_calls.data(), pass.draw_calls.size()}, pass.viewport);
            }

            backend_->end_pass();

            if (is_texture) {
                had_texture_pass = true;
            }
        }
        {
            VELK_PERF_SCOPE("renderer.end_frame");
            backend_->end_frame();
        }
        present_counter_++;
        target->ready = false;
        target->presented_at = present_counter_;
        target->passes.clear();

        slot_cv_.notify_one();
    }
    VELK_PERF_EVENT(Present);
}

void Renderer::render()
{
    present(prepare({}));
}

void Renderer::set_max_frames_in_flight(uint32_t count)
{
    if (count < 1) {
        count = 1;
    }
    std::lock_guard<std::mutex> lock(slot_mutex_);
    auto old_size = frame_slots_.size();
    frame_slots_.resize(count);
    for (auto i = old_size; i < count; ++i) {
        frame_buffer_.init_slot(frame_slots_[i].buffer, *backend_);
    }
    slot_cv_.notify_all();
}

void Renderer::on_gpu_resource_destroyed(IGpuResource* resource)
{
    resources_.on_resource_destroyed(resource, present_counter_, kGpuLatencyFrames);
}

void Renderer::shutdown()
{
    if (backend_) {
        // Detach from any GPU resources we are still observing so their
        // dtors (which may run later, on any thread) cannot reach a dead
        // renderer.
        for (auto& [elem, cache] : batch_builder_.element_cache()) {
            for (auto& weak : cache.gpu_resources) {
                if (auto res = weak.lock()) {
                    res->remove_gpu_resource_observer(this);
                }
            }
        }
        // Unregister from environment textures (not tracked via element cache).
        resources_.unregister_env_observers(this);

        resources_.shutdown(*backend_);

        for (auto& entry : views_) {
            if (entry.rt_output_tex != 0) {
                backend_->destroy_texture(entry.rt_output_tex);
                entry.rt_output_tex = 0;
            }
            backend_->destroy_surface(get_render_target_id(entry.surface));
        }

        for (auto& [key, rte] : render_target_entries_) {
            if (rte.texture_id != 0) {
                backend_->destroy_texture(rte.texture_id);
            }
        }
        render_target_entries_.clear();

        for (auto& slot : frame_slots_) {
            if (slot.buffer.handle) {
                backend_->destroy_buffer(slot.buffer.handle);
                slot.buffer.handle = 0;
            }
        }

        backend_ = nullptr;
    }

    views_.clear();
    batch_builder_.clear();
    draw_calls_.clear();
    pipeline_map_ = nullptr;
}

uint64_t Renderer::ensure_rt_pipeline(const vector<uint32_t>& material_ids)
{
    if (!render_ctx_) {
        return 0;
    }

    // Pipeline cache key: FNV-1a on the sorted material-id list. 0 material
    // ids hashes to the FNV basis; so the "no materials" pipeline is stable.
    constexpr uint64_t kFnvBasis = 0xcbf29ce484222325ULL;
    constexpr uint64_t kFnvPrime = 0x100000001b3ULL;
    constexpr uint64_t kRtTag = 0x5274436f6d702000ULL; // "RtComp \0" to avoid collisions
    uint64_t key = kFnvBasis ^ kRtTag;
    for (auto id : material_ids) {
        key = (key ^ static_cast<uint64_t>(id)) * kFnvPrime;
    }
    // Avoid collision with the render-context's auto-assign counter range
    // (PipelineKey::CustomBase..). Sentinel bit forces the key well away.
    key |= 0x8000000000000000ULL;

    auto cached = rt_compiled_pipelines_.find(key);
    if (cached != rt_compiled_pipelines_.end()) {
        return key;
    }

    // Compose source: prelude + #include each active material's snippet
    // + generated dispatch + main.
    string src;
    src += rt_compute_prelude_src;
    for (auto id : material_ids) {
        if (id == 0 || id > rt_material_info_by_id_.size()) continue;
        const auto& mi = rt_material_info_by_id_[id - 1];
        src += string_view("#include \"", 10);
        src += mi.include_name;
        src += string_view("\"\n", 2);
    }

    auto append_literal = [&src](const char* s) {
        src += string_view(s, std::strlen(s));
    };

    append_literal(
        "vec4 velk_resolve_fill(uint mid, uint64_t addr, uint tex_id, uint shape_param,"
        " vec2 uv, vec4 base, vec3 ray_dir) {\n");
    append_literal("    switch (mid) {\n");
    char buf[128];
    for (auto id : material_ids) {
        if (id == 0 || id > rt_material_info_by_id_.size()) continue;
        const auto& mi = rt_material_info_by_id_[id - 1];
        int n = std::snprintf(buf, sizeof(buf), "        case %uu: return ", id);
        if (n > 0) {
            src += string_view(static_cast<const char*>(buf), static_cast<size_t>(n));
        }
        src += mi.fill_fn_name;
        append_literal("(addr, tex_id, shape_param, uv, base, ray_dir);\n");
    }
    append_literal("        default: return base;\n");
    append_literal("    }\n");
    append_literal("}\n");
    src += rt_compute_main_src;

    uint64_t compiled = render_ctx_->compile_compute_pipeline(string_view(src), key);
    if (compiled == 0) {
        return 0;
    }
    rt_compiled_pipelines_[key] = true;
    return key;
}

void Renderer::prepend_environment_batch(ICamera& camera, ViewEntry& entry)
{
    if (!backend_) {
        return;
    }

    auto cam_state = read_state<ICamera>(&camera);
    if (!(cam_state && cam_state->environment)) {
        return;
    }
    auto env_ptr = cam_state->environment.get<IEnvironment>();
    if (!env_ptr) {
        return;
    }
    auto* surf = interface_cast<ISurface>(env_ptr);
    auto* buf = interface_cast<IBuffer>(env_ptr);
    if (!surf || !buf) {
        return;
    }

    // Upload the environment texture if dirty.
    if (buf->is_dirty()) {
        const uint8_t* pixels = buf->get_data();
        auto sz = surf->get_dimensions();
        int tw = static_cast<int>(sz.x);
        int th = static_cast<int>(sz.y);
        if (pixels && tw > 0 && th > 0) {
            TextureId tid = resources_.find_texture(surf);
            if (tid == 0) {
                TextureDesc desc{};
                desc.width = tw;
                desc.height = th;
                desc.format = surf->format();
                tid = backend_->create_texture(desc);
                resources_.register_texture(surf, tid);
                surf->add_gpu_resource_observer(this);
                auto buf_ptr = interface_pointer_cast<IBuffer>(env_ptr);
                if (buf_ptr) {
                    resources_.add_env_observer(buf_ptr);
                }
            }
            if (tid != 0) {
                backend_->upload_texture(tid, pixels, tw, th);
            }
            buf->clear_dirty();
        }
    }

    // Get the material from the environment (owned by the environment,
    // like Font owns TextMaterial).
    auto material = env_ptr->get_material();
    if (!material) {
        return;
    }

    // Insert a synthetic batch at the front of batches_ so the environment
    // renders before all scene geometry. The batch flows through the
    // normal build_draw_calls path. The env vertex shader generates a
    // fullscreen quad from vertex index; no instance data is needed, but
    // the batch must have instance_count = 1 to produce a draw call.
    BatchBuilder::Batch env_batch;
    env_batch.pipeline_key = 0; // material override supplies the pipeline
    env_batch.texture_key = reinterpret_cast<uint64_t>(surf);
    env_batch.instance_stride = 4;
    env_batch.instance_count = 1;
    env_batch.instance_data.resize(4, 0); // dummy, shader ignores it
    env_batch.material = std::move(material);

    entry.batches.insert(entry.batches.begin(), std::move(env_batch));
}

} // namespace velk::ui
