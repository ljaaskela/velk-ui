#include "renderer.h"

#include <velk/api/any.h>
#include <velk/api/state.h>
#include <velk/api/velk.h>
#include <velk/interface/intf_object_storage.h>
#include <velk-ui/interface/intf_visual.h>

#include <cstring>

#ifdef VELK_RENDER_DEBUG
#define RENDER_LOG(...) VELK_LOG(I, __VA_ARGS__)
#else
#define RENDER_LOG(...) ((void)0)
#endif

namespace velk_ui {

namespace {

uint64_t make_batch_key(uint64_t pipeline, uint64_t format, uint64_t texture)
{
    return (pipeline * 31 + format) * 31 + texture;
}

void pack_instance(velk::vector<uint8_t>& buf, const float* data, uint32_t stride)
{
    auto offset = buf.size();
    buf.resize(offset + stride);
    std::memcpy(buf.data() + offset, data, stride);
}

} // namespace

void Renderer::set_backend(const IRenderBackend::Ptr& backend, IRenderContext* ctx)
{
    backend_ = backend;
    render_ctx_ = ctx;
}

void Renderer::attach(const ISurface::Ptr& surface, const IScene::Ptr& scene)
{
    if (!(surface && scene)) {
        VELK_LOG(E, "Renderer::attach: surface and scene must be valid objects");
        return;
    }

    for (auto& entry : surfaces_) {
        if (entry.surface == surface) {
            entry.scene = scene;
            return;
        }
    }

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

            auto vstate = velk::read_state<IVisual>(visual);
            auto mat_obj = (vstate && vstate->paint) ? vstate->paint.get() : velk::IObject::Ptr{};
            auto* mat = mat_obj ? interface_cast<IMaterial>(mat_obj) : nullptr;
            if (mat && render_ctx_) {
                uint64_t handle = mat->get_pipeline_handle(*render_ctx_);
                if (handle != 0) {
                    vc.pipeline_key = handle;
                    vc.material = mat;
                    populate_uniform_bindings(vc, mat);
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
            } else if (cmd.type == DrawCommandType::FillRoundedRect) {
                if (pipeline == 0) pipeline = PipelineKey::RoundedRect;
                format = VertexFormat::Untextured;
            } else if (cmd.type == DrawCommandType::TexturedQuad) {
                if (pipeline == 0) pipeline = PipelineKey::Text;
                format = VertexFormat::Textured;
                texture = cache.texture_provider
                    ? reinterpret_cast<uint64_t>(cache.texture_provider)
                    : TextureKey::Atlas;
            } else {
                continue;
            }

            bool needs_rect = (pipeline >= PipelineKey::CustomBase)
                || (cmd.type == DrawCommandType::FillRoundedRect);

            uint64_t bkey = needs_rect
                ? make_batch_key(pipeline, format, reinterpret_cast<uintptr_t>(element))
                : make_batch_key(pipeline, format, texture);

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

            if (needs_rect && !batch.has_rect) {
                batch.rect = {x, y, w, h};
                batch.has_rect = true;

                // Fill material uniforms for this batch (once per batch creation)
                if (vc.material) {
                    fill_batch_uniforms(batch, vc, x, y, w, h);
                }
            }

            batch.instance_count++;
        }
        } // for each visual
    }
}

void Renderer::render()
{
    if (!backend_) {
        return;
    }

    for (auto& entry : surfaces_) {
        auto* scene = interface_cast<IScene>(entry.scene);
        if (!scene) {
            continue;
        }

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

        for (auto& removed : state.removed_list) {
            auto* elem = interface_cast<IElement>(removed);
            if (elem) {
                element_cache_.erase(elem);
            }
        }

        for (auto* element : state.redraw_list) {
            rebuild_commands(element);
        }

        bool textures_uploaded = false;
        if (has_changes) {
            for (auto& [elem, cache] : element_cache_) {
                if (cache.texture_provider && cache.texture_provider->is_texture_dirty()) {
                    auto* tp = cache.texture_provider;
                    uint32_t tw = tp->get_texture_width();
                    uint32_t th = tp->get_texture_height();
                    const uint8_t* pixels = tp->get_pixels();
                    if (pixels && tw > 0 && th > 0) {
                        uint64_t tex_key = reinterpret_cast<uint64_t>(tp);
                        backend_->upload_texture(tex_key, pixels,
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

        if (entry.batches_dirty) {
            rebuild_batches(state, entry);
            entry.batches_dirty = false;

            RENDER_LOG("render: rebuilt batches=%zu instances=%u",
                       batches_.size(),
                       [&]{ uint32_t n = 0; for (auto& b : batches_) n += b.instance_count; return n; }());
        }

        backend_->begin_frame(entry.surface_id);
        backend_->submit({batches_.data(), batches_.size()});
        backend_->end_frame();
    }
}

void Renderer::populate_uniform_bindings(VisualCommands& vc, IMaterial* mat)
{
    if (!backend_) return;

    uint64_t key = vc.pipeline_key;

    // Get or cache pipeline uniforms
    auto uit = pipeline_uniforms_.find(key);
    if (uit == pipeline_uniforms_.end()) {
        pipeline_uniforms_[key] = backend_->get_pipeline_uniforms(key);
        uit = pipeline_uniforms_.find(key);
    }

    auto* meta = interface_cast<velk::IMetadata>(mat);
    if (!meta) return;

    RENDER_LOG("populate_uniform_bindings: pipeline=%llu, uniform_count=%zu",
               key, uit->second.size());

    vc.uniform_bindings.clear();
    for (auto& info : uit->second) {
        // Skip renderer-managed uniforms
        if (info.name == "u_projection" || info.name == "u_rect") {
            continue;
        }

        // Try to find a property matching the uniform name (strip u_ prefix)
        velk::string_view prop_name = info.name;
        if (prop_name.size() > 2 && prop_name[0] == 'u' && prop_name[1] == '_') {
            prop_name = velk::string_view(prop_name.data() + 2, prop_name.size() - 2);
        }

        auto prop = meta->get_property(prop_name, velk::Resolve::Existing);
        if (!prop) {
            prop = meta->get_property(prop_name, velk::Resolve::Create);
        }

        if (prop) {
            RENDER_LOG("  bound uniform '%.*s' -> property '%.*s' loc=%d",
                       static_cast<int>(info.name.size()), info.name.data(),
                       static_cast<int>(prop_name.size()), prop_name.data(),
                       info.location);
            UniformBinding binding;
            binding.location = info.location;
            binding.typeUid = info.typeUid;
            binding.property = prop;
            vc.uniform_bindings.push_back(std::move(binding));
        } else {
            RENDER_LOG("  uniform '%.*s' -> no property '%.*s'",
                       static_cast<int>(info.name.size()), info.name.data(),
                       static_cast<int>(prop_name.size()), prop_name.data());
        }
    }
}

void Renderer::fill_batch_uniforms(RenderBatch& batch, const VisualCommands& vc,
                                   float x, float y, float w, float h) const
{
    // Projection uniform
    auto proj_it = pipeline_uniforms_.find(batch.pipeline_key);
    if (proj_it != pipeline_uniforms_.end()) {
        for (auto& info : proj_it->second) {
            if (info.name == "u_projection") {
                // Projection is set per-frame; we don't have it here.
                // The backend still needs it. We'll pack it from the entry.
                // For now, leave it for the caller to set.
            } else if (info.name == "u_rect") {
                UniformValue uv;
                uv.location = info.location;
                uv.typeUid = info.typeUid;
                uv.data[0] = x;
                uv.data[1] = y;
                uv.data[2] = w;
                uv.data[3] = h;
                batch.uniforms.push_back(uv);
            }
        }
    }

    // Material property uniforms
    for (auto& binding : vc.uniform_bindings) {
        if (!binding.property) continue;

        UniformValue uv;
        uv.location = binding.location;
        uv.typeUid = binding.typeUid;

        auto val = binding.property->get_value();
        if (val) {
            size_t data_size = val->get_data_size(binding.typeUid);
            if (data_size > 0 && data_size <= sizeof(uv.data)) {
                val->get_data(uv.data, data_size, binding.typeUid);
            }
        }

        batch.uniforms.push_back(uv);
    }
}

void Renderer::shutdown()
{
    if (backend_) {
        for (auto& entry : surfaces_) {
            backend_->destroy_surface(entry.surface_id);
        }
        backend_ = nullptr;
    }

    surfaces_.clear();
    element_cache_.clear();
    batch_index_.clear();
    batches_.clear();
    pipeline_uniforms_.clear();
}

} // namespace velk_ui
