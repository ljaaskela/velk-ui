#include "deferred_gbuffer_path.h"

#include "render_target_cache.h"

#include <velk/api/perf.h>
#include <velk/api/state.h>
#include <velk/interface/intf_object_storage.h>

#include <velk-render/gbuffer.h>
#include <velk-render/gpu_data.h>
#include <velk-render/interface/material/intf_material.h>
#include <velk-render/interface/intf_camera.h>
#include <velk-render/interface/intf_surface.h>

#include <cstring>

namespace velk {

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

void DeferredGBufferPath::build_passes(ViewEntry& entry,
                                       const SceneState& scene_state,
                                       FrameContext& ctx,
                                       vector<RenderPass>& out_passes)
{
    if (!ctx.backend || !ctx.frame_buffer || !ctx.batch_builder || !ctx.pipeline_map) {
        return;
    }

    auto camera = ::velk::find_attachment<ICamera>(entry.camera_element);

    if (entry.batches_dirty) {
        ctx.batch_builder->rebuild_batches(scene_state, entry.batches);
        entry.batches_dirty = false;
    }

    if (ctx.render_target_cache) {
        ctx.render_target_cache->ensure(ctx);
    }

    auto sstate = read_state<IWindowSurface>(entry.surface);
    float sw = static_cast<float>(sstate ? sstate->size.x : 0);
    float sh = static_cast<float>(sstate ? sstate->size.y : 0);
    bool has_viewport = entry.viewport.width > 0 && entry.viewport.height > 0;
    float vp_w = has_viewport ? entry.viewport.width * sw : sw;
    float vp_h = has_viewport ? entry.viewport.height * sh : sh;

    uint64_t globals_gpu_addr = 0;
    mat4 vp_mat = mat4::identity();
    if (vp_w > 0 && vp_h > 0) {
        FrameGlobals globals{};
        if (camera) {
            auto cam_es = read_state<IElement>(entry.camera_element);
            mat4 cam_world = cam_es ? cam_es->world_matrix : mat4::identity();
            vp_mat = camera->get_view_projection(cam_world, vp_w, vp_h);
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
        if (camera) {
            auto cam_es = read_state<IElement>(entry.camera_element);
            if (cam_es) {
                globals.cam_pos[0] = cam_es->world_matrix(0, 3);
                globals.cam_pos[1] = cam_es->world_matrix(1, 3);
                globals.cam_pos[2] = cam_es->world_matrix(2, 3);
            }
        }
        globals.bvh_root = ctx.bvh_root;
        globals.bvh_node_count = ctx.bvh_node_count;
        globals.bvh_shape_count = ctx.bvh_shape_count;
        globals.bvh_nodes_addr = ctx.bvh_nodes_addr;
        globals.bvh_shapes_addr = ctx.bvh_shapes_addr;
        globals_gpu_addr = ctx.frame_buffer->write(&globals, sizeof(globals));
    }
    entry.frame_globals_addr = globals_gpu_addr;

    ::velk::render::Frustum frustum;
    const ::velk::render::Frustum* frustum_ptr = nullptr;
    if (camera && vp_w > 0 && vp_h > 0) {
        frustum = ::velk::render::extract_frustum(vp_mat);
        frustum_ptr = &frustum;
    }

    auto& vs = view_states_[&entry];
    auto gbuffer_group = ensure_gbuffer(entry, vs, static_cast<int>(vp_w),
                                        static_cast<int>(vp_h), ctx);
    if (gbuffer_group == 0) return;

    emit_pass(entry, vs, ctx, globals_gpu_addr, frustum_ptr, out_passes);
}

RenderTargetGroup DeferredGBufferPath::ensure_gbuffer(ViewEntry& /*view*/, ViewState& vs,
                                                     int width, int height,
                                                     FrameContext& ctx)
{
    if (width <= 0 || height <= 0 || !ctx.backend) {
        return 0;
    }
    if (vs.gbuffer_group != 0 && vs.width == width && vs.height == height) {
        return vs.gbuffer_group;
    }
    if (vs.gbuffer_group != 0) {
        ctx.backend->destroy_render_target_group(vs.gbuffer_group);
        vs.gbuffer_group = 0;
    }
    vs.gbuffer_group = ctx.backend->create_render_target_group(
        array_view<const PixelFormat>(kGBufferFormats,
                                      static_cast<uint32_t>(GBufferAttachment::Count)),
        width, height,
        DepthFormat::Default);
    if (vs.gbuffer_group != 0) {
        vs.width = width;
        vs.height = height;
    }
    return vs.gbuffer_group;
}

void DeferredGBufferPath::emit_pass(ViewEntry& entry, ViewState& vs,
                                    FrameContext& ctx,
                                    uint64_t globals_gpu_addr,
                                    const ::velk::render::Frustum* frustum,
                                    vector<RenderPass>& out_passes)
{
    vector<DrawCall> gbuffer_draw_calls;
    ctx.batch_builder->build_gbuffer_draw_calls(entry.batches,
                                                gbuffer_draw_calls,
                                                *ctx.frame_buffer,
                                                *ctx.resources,
                                                globals_gpu_addr,
                                                ctx.render_ctx,
                                                vs.gbuffer_group,
                                                ctx.observer,
                                                frustum);

    RenderPass g_pass;
    g_pass.kind = PassKind::GBufferFill;
    g_pass.gbuffer_group = vs.gbuffer_group;
    g_pass.viewport = {0, 0, static_cast<float>(vs.width),
                       static_cast<float>(vs.height)};
    g_pass.draw_calls = std::move(gbuffer_draw_calls);
    out_passes.push_back(std::move(g_pass));
}

void DeferredGBufferPath::on_view_removed(ViewEntry& view, FrameContext& ctx)
{
    auto it = view_states_.find(&view);
    if (it == view_states_.end()) return;
    if (it->second.gbuffer_group != 0 && ctx.backend) {
        ctx.backend->destroy_render_target_group(it->second.gbuffer_group);
    }
    view_states_.erase(it);
}

void DeferredGBufferPath::shutdown(FrameContext& ctx)
{
    if (ctx.backend) {
        for (auto& [v, vs] : view_states_) {
            if (vs.gbuffer_group != 0) {
                ctx.backend->destroy_render_target_group(vs.gbuffer_group);
            }
        }
    }
    view_states_.clear();
}

RenderTargetGroup DeferredGBufferPath::find_gbuffer_group(ViewEntry* view) const
{
    auto it = view_states_.find(view);
    return (it == view_states_.end()) ? 0 : it->second.gbuffer_group;
}

int DeferredGBufferPath::find_gbuffer_width(ViewEntry* view) const
{
    auto it = view_states_.find(view);
    return (it == view_states_.end()) ? 0 : it->second.width;
}

int DeferredGBufferPath::find_gbuffer_height(ViewEntry* view) const
{
    auto it = view_states_.find(view);
    return (it == view_states_.end()) ? 0 : it->second.height;
}

} // namespace velk
