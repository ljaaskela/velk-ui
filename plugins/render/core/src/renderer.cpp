#include "renderer.h"
#include "surface.h"

#include <velk/api/state.h>
#include <velk/api/velk.h>
#include <velk/interface/intf_object_storage.h>
#include <velk-ui/interface/intf_material.h>
#include <velk-ui/interface/intf_visual.h>

#include <cstring>

#ifdef VELK_RENDER_DEBUG
#define RENDER_LOG(...) VELK_LOG(I, __VA_ARGS__)
#else
#define RENDER_LOG(...) ((void)0)
#endif

namespace velk_ui {

namespace {

// Default shader sources

const char* rect_vertex_src = R"(
#version 330 core

const vec2 quad[4] = vec2[4](
    vec2(0.0, 0.0),
    vec2(1.0, 0.0),
    vec2(0.0, 1.0),
    vec2(1.0, 1.0)
);

layout(location = 0) in vec4 inst_rect;
layout(location = 1) in vec4 inst_color;

uniform mat4 u_projection;

out vec4 v_color;

void main()
{
    vec2 pos = quad[gl_VertexID];
    vec2 world_pos = inst_rect.xy + pos * inst_rect.zw;
    gl_Position = u_projection * vec4(world_pos, 0.0, 1.0);
    v_color = inst_color;
}
)";

const char* rect_fragment_src = R"(
#version 330 core

in vec4 v_color;
out vec4 frag_color;

void main()
{
    frag_color = v_color;
}
)";

const char* text_vertex_src = R"(
#version 330 core

const vec2 quad[4] = vec2[4](
    vec2(0.0, 0.0),
    vec2(1.0, 0.0),
    vec2(0.0, 1.0),
    vec2(1.0, 1.0)
);

layout(location = 0) in vec4 inst_rect;
layout(location = 1) in vec4 inst_color;
layout(location = 2) in vec4 inst_uv;

uniform mat4 u_projection;

out vec4 v_color;
out vec2 v_uv;

void main()
{
    vec2 pos = quad[gl_VertexID];
    vec2 world_pos = inst_rect.xy + pos * inst_rect.zw;
    gl_Position = u_projection * vec4(world_pos, 0.0, 1.0);
    v_color = inst_color;
    v_uv.x = mix(inst_uv.x, inst_uv.z, pos.x);
    v_uv.y = mix(inst_uv.y, inst_uv.w, pos.y);
}
)";

const char* text_fragment_src = R"(
#version 330 core

uniform sampler2D u_atlas;

in vec4 v_color;
in vec2 v_uv;
out vec4 frag_color;

void main()
{
    float alpha = texture(u_atlas, v_uv).r;
    frag_color = vec4(v_color.rgb, v_color.a * alpha);
}
)";

uint64_t hash_string(velk::string_view s)
{
    // FNV-1a 64-bit
    uint64_t h = 14695981039346656037ULL;
    for (size_t i = 0; i < s.size(); ++i) {
        h ^= static_cast<uint64_t>(static_cast<uint8_t>(s[i]));
        h *= 1099511628211ULL;
    }
    return h;
}

uint64_t make_batch_key(uint64_t pipeline, uint64_t format, uint64_t texture)
{
    // Combine three keys into one composite key for batch lookup
    return (pipeline * 31 + format) * 31 + texture;
}

void pack_instance(velk::vector<uint8_t>& buf, const float* data, uint32_t stride)
{
    auto offset = buf.size();
    buf.resize(offset + stride);
    std::memcpy(buf.data() + offset, data, stride);
}

} // namespace

bool Renderer::init(const RenderConfig& config)
{
    // Well-known backend UIDs (avoids compile-time dependency on backend plugins)
    static constexpr velk::Uid kGlPluginId{"e1e9e004-21cd-4cfa-b843-49b0eb358149"};
    static constexpr velk::Uid kGlBackendId{"2302c979-1531-4d0b-bab6-d1bac99f0a11"};

    velk::Uid plugin_id;
    velk::Uid class_id;

    switch (config.backend) {
    case RenderBackendType::GL:
        plugin_id = kGlPluginId;
        class_id = kGlBackendId;
        break;
    default:
        VELK_LOG(E, "Renderer::init: unsupported backend type %d", static_cast<int>(config.backend));
        return false;
    }

    auto& reg = velk::instance().plugin_registry();
    if (!reg.get_or_load_plugin(plugin_id)) {
        VELK_LOG(E, "Renderer::init: failed to load backend plugin");
        return false;
    }

    auto obj = velk::instance().create<velk::IObject>(class_id);
    backend_ = interface_pointer_cast<IRenderBackend>(obj);
    if (!backend_) {
        VELK_LOG(E, "Renderer::init: failed to create backend");
        return false;
    }

    if (!backend_->init()) {
        VELK_LOG(E, "Renderer::init: backend init failed");
        backend_ = nullptr;
        return false;
    }

    // Register default pipelines
    PipelineDesc rect_desc{rect_vertex_src, rect_fragment_src, VertexFormat::Untextured};
    backend_->register_pipeline(PipelineKey::Rect, rect_desc);

    PipelineDesc text_desc{text_vertex_src, text_fragment_src, VertexFormat::Textured};
    backend_->register_pipeline(PipelineKey::Text, text_desc);

    initialized_ = true;
    VELK_LOG(I, "Renderer initialized (backend=%d)", static_cast<int>(config.backend));
    return true;
}

ISurface::Ptr Renderer::create_surface(int width, int height)
{
    auto obj = velk::instance().create<velk::IObject>(Surface::static_class_id());
    auto surface = interface_pointer_cast<ISurface>(obj);
    if (!surface) {
        return nullptr;
    }

    velk::write_state<ISurface>(surface, [&](ISurface::State& s) {
        s.width = width;
        s.height = height;
    });

    if (backend_) {
        uint64_t sid = next_surface_id_++;
        SurfaceDesc desc{width, height};
        backend_->create_surface(sid, desc);

        // Store the surface id somewhere we can retrieve it
        // We'll use the surfaces_ vector to map surface ptr -> id
        surfaces_.push_back({surface, nullptr, sid});
    }

    return surface;
}

void Renderer::attach(const ISurface::Ptr& surface, const velk::IInterface::Ptr& scene_ptr)
{
    if (!surface) {
        return;
    }

    auto scene = interface_pointer_cast<IScene>(scene_ptr);
    if (!scene) {
        VELK_LOG(E, "Renderer::attach: object does not implement IScene");
        return;
    }

    for (auto& entry : surfaces_) {
        if (entry.surface == surface) {
            entry.scene = scene;
            return;
        }
    }

    // Surface wasn't created through create_surface, create backend surface now
    auto state = velk::read_state<ISurface>(surface);
    uint64_t sid = next_surface_id_++;
    if (backend_ && state) {
        SurfaceDesc desc{state->width, state->height};
        backend_->create_surface(sid, desc);
    }
    surfaces_.push_back({surface, scene, sid});
}

void Renderer::detach(const ISurface::Ptr& surface)
{
    for (auto it = surfaces_.begin(); it != surfaces_.end(); ++it) {
        if (it->surface == surface) {
            if (backend_) {
                backend_->destroy_surface(it->surface_id);
            }
            surfaces_.erase(it);
            return;
        }
    }
}

void Renderer::rebuild_commands(IElement* element)
{
    auto& cache = element_cache_[element];
    cache.visuals.clear();
    cache.texture_provider = nullptr;

    auto* storage = interface_cast<velk::IObjectStorage>(element);
    if (!storage) {
        return;
    }

    auto state = velk::read_state<IElement>(element);
    if (!state) {
        return;
    }

    velk::rect local_rect = {0, 0, state->size.width, state->size.height};

    for (size_t i = 0; i < storage->attachment_count(); ++i) {
        auto att = storage->get_attachment(i);

        auto* visual = interface_cast<IVisual>(att);
        if (visual) {
            VisualCommands vc;
            auto commands = visual->get_draw_commands(local_rect);
            for (auto& cmd : commands) {
                vc.commands.push_back(cmd);
            }

            // Check for custom material shader
            auto vstate = velk::read_state<IVisual>(visual);
            auto mat_obj = (vstate && vstate->paint) ? vstate->paint.get() : velk::IObject::Ptr{};
            auto* mat = mat_obj ? interface_cast<IMaterial>(mat_obj) : nullptr;
            if (mat) {
                auto mstate = velk::read_state<IMaterial>(mat);
                if (mstate && !mstate->fragment_source.empty()) {
                    auto src_hash = hash_string(mstate->fragment_source);
                    auto it = material_hash_to_pipeline_.find(src_hash);
                    if (it != material_hash_to_pipeline_.end()) {
                        vc.pipeline_key = it->second;
                    } else {
                        uint64_t key = next_pipeline_key_++;
                        material_hash_to_pipeline_[src_hash] = key;
                        vc.pipeline_key = key;

                        if (backend_) {
                            PipelineDesc desc{
                                rect_vertex_src,
                                mstate->fragment_source.c_str(),
                                VertexFormat::Untextured};
                            backend_->register_pipeline(key, desc);
                        }
                    }
                }
            }

            cache.visuals.push_back(std::move(vc));
        }

        auto* tp = interface_cast<ITextureProvider>(att);
        if (tp) {
            cache.texture_provider = tp;
        }
    }
}

void Renderer::rebuild_batches(const SceneState& state, const SurfaceEntry& entry)
{
    batches_.clear();
    batch_index_.clear();

    for (auto* element : state.visual_list) {
        auto it = element_cache_.find(element);
        if (it == element_cache_.end()) {
            continue;
        }

        auto& cache = it->second;
        auto elem_state = velk::read_state<IElement>(element);
        if (!elem_state) {
            continue;
        }

        float wx = elem_state->world_matrix(0, 3);
        float wy = elem_state->world_matrix(1, 3);

        for (auto& vc : cache.visuals) {
        for (auto& cmd : vc.commands) {
            uint64_t pipeline = vc.pipeline_key;
            uint64_t format = 0;
            uint64_t texture = 0;

            if (cmd.type == DrawCommandType::FillRect) {
                if (pipeline == 0) pipeline = PipelineKey::Rect;
                format = VertexFormat::Untextured;
            } else if (cmd.type == DrawCommandType::TexturedQuad) {
                if (pipeline == 0) pipeline = PipelineKey::Text;
                format = VertexFormat::Textured;
                texture = TextureKey::Atlas;
            } else {
                continue;
            }

            bool is_custom = (pipeline >= PipelineKey::CustomBase);

            // For custom pipelines, each element gets its own batch (for u_rect uniform).
            // For default pipelines, merge by (pipeline, format, texture).
            uint64_t bkey = is_custom
                ? make_batch_key(pipeline, format, reinterpret_cast<uintptr_t>(element))
                : make_batch_key(pipeline, format, texture);

            // Find or create batch
            auto bit = batch_index_.find(bkey);
            size_t batch_idx;
            if (bit != batch_index_.end()) {
                batch_idx = bit->second;
            } else {
                batch_idx = batches_.size();
                batch_index_[bkey] = batch_idx;
                RenderBatch batch;
                batch.pipeline_key = pipeline;
                batch.vertex_format_key = format;
                batch.texture_key = texture;
                batch.instance_stride = (format == VertexFormat::Untextured)
                    ? VertexFormat::UntexturedStride : VertexFormat::TexturedStride;
                batches_.push_back(std::move(batch));
            }

            auto& batch = batches_[batch_idx];

            float x = wx + cmd.bounds.x;
            float y = wy + cmd.bounds.y;
            float w = cmd.bounds.width;
            float h = cmd.bounds.height;

            if (format == VertexFormat::Textured) {
                float data[] = {x, y, w, h,
                                cmd.color.r, cmd.color.g, cmd.color.b, cmd.color.a,
                                cmd.u0, cmd.v0, cmd.u1, cmd.v1};
                pack_instance(batch.instance_data, data, VertexFormat::TexturedStride);
            } else {
                float data[] = {x, y, w, h,
                                cmd.color.r, cmd.color.g, cmd.color.b, cmd.color.a};
                pack_instance(batch.instance_data, data, VertexFormat::UntexturedStride);
            }

            if (is_custom && !batch.has_rect) {
                batch.rect = {x, y, w, h};
                batch.has_rect = true;
            }

            batch.instance_count++;
        }
        } // for each visual
    }
}

void Renderer::render()
{
    if (!initialized_ || !backend_) {
        return;
    }

    for (auto& entry : surfaces_) {
        auto* scene = interface_cast<IScene>(entry.scene);
        if (!scene) {
            continue;
        }

        // Check for surface resize
        auto sstate = velk::read_state<ISurface>(entry.surface);
        if (sstate) {
            if (sstate->width != entry.cached_width || sstate->height != entry.cached_height) {
                entry.cached_width = sstate->width;
                entry.cached_height = sstate->height;
                SurfaceDesc desc{sstate->width, sstate->height};
                backend_->update_surface(entry.surface_id, desc);
                entry.batches_dirty = true;
                RENDER_LOG("render: surface resized to %dx%d", sstate->width, sstate->height);
            }
        }

        auto state = scene->consume_state();

        bool has_changes = !state.redraw_list.empty() || !state.removed_list.empty();

        if (has_changes) {
            RENDER_LOG("render: redraw=%zu removed=%zu cache=%zu",
                       state.redraw_list.size(), state.removed_list.size(),
                       element_cache_.size());
        }

        // Process removals
        for (auto& removed : state.removed_list) {
            auto* elem = interface_cast<IElement>(removed);
            if (elem) {
                element_cache_.erase(elem);
            }
        }

        // Rebuild draw commands for changed elements
        for (auto* element : state.redraw_list) {
            rebuild_commands(element);
        }

        // Handle texture uploads (only scan when elements changed)
        bool textures_uploaded = false;
        if (has_changes) {
            for (auto& [elem, cache] : element_cache_) {
                if (cache.texture_provider && cache.texture_provider->is_texture_dirty()) {
                    auto* tp = cache.texture_provider;
                    uint32_t tw = tp->get_texture_width();
                    uint32_t th = tp->get_texture_height();
                    const uint8_t* pixels = tp->get_pixels();
                    if (pixels && tw > 0 && th > 0) {
                        backend_->upload_texture(TextureKey::Atlas, pixels,
                                                 static_cast<int>(tw), static_cast<int>(th));
                        tp->clear_texture_dirty();
                        textures_uploaded = true;
                    }
                }
            }
        }

        if (has_changes || textures_uploaded) {
            entry.batches_dirty = true;
        }

        // Rebuild batches only when something changed
        if (entry.batches_dirty) {
            rebuild_batches(state, entry);
            entry.batches_dirty = false;

            RENDER_LOG("render: rebuilt batches=%zu instances=%u",
                       batches_.size(),
                       [&]{ uint32_t n = 0; for (auto& b : batches_) n += b.instance_count; return n; }());
        }

        // Always submit (framebuffer is swapped each frame)
        backend_->begin_frame(entry.surface_id);
        backend_->submit({batches_.data(), batches_.size()});
        backend_->end_frame();
    }
}

void Renderer::shutdown()
{
    if (backend_) {
        for (auto& entry : surfaces_) {
            backend_->destroy_surface(entry.surface_id);
        }
        backend_->shutdown();
        backend_ = nullptr;
    }

    surfaces_.clear();
    element_cache_.clear();
    batch_index_.clear();
    batches_.clear();
    material_hash_to_pipeline_.clear();
    initialized_ = false;
}

} // namespace velk_ui
