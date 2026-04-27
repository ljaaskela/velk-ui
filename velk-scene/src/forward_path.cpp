#include "forward_path.h"

#include "env_helper.h"
#include "render_target_cache.h"

#include <velk/api/perf.h>
#include <velk/api/state.h>
#include <velk/interface/intf_object_storage.h>

#include <velk-render/gpu_data.h>
#include <velk-render/interface/material/intf_material.h>
#include <velk-render/interface/intf_surface.h>
#include <velk-scene/interface/intf_environment.h>
#include <velk-scene/interface/intf_render_to_texture.h>

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

void ForwardPath::prepend_environment_batch(ICamera& camera, ViewState& vs, FrameContext& ctx)
{
    auto resolved = ensure_env_ready(camera, ctx);
    if (!resolved.env || !resolved.surface) {
        return;
    }

    auto material = resolved.env->get_material();
    if (!material) {
        return;
    }

    BatchBuilder::Batch env_batch;
    env_batch.pipeline_key = 0;
    env_batch.texture_key = reinterpret_cast<uint64_t>(resolved.surface);
    env_batch.instance_stride = 4;
    env_batch.instance_count = 1;
    env_batch.instance_data.resize(4, 0);
    env_batch.material = std::move(material);
    if (ctx.render_ctx) {
        auto quad = ctx.render_ctx->get_mesh_builder().get_unit_quad();
        if (quad) {
            auto prims = quad->get_primitives();
            if (prims.size() > 0) env_batch.primitive = prims[0];
        }
    }

    vs.batches.insert(vs.batches.begin(), std::move(env_batch));
}

void ForwardPath::build_passes(ViewEntry& entry,
                               const SceneState& scene_state,
                               FrameContext& ctx,
                               vector<RenderPass>& out_passes)
{
    if (!ctx.backend || !ctx.frame_buffer || !ctx.batch_builder || !ctx.pipeline_map) {
        return;
    }

    auto camera = ::velk::find_attachment<ICamera>(entry.camera_element);
    auto& vs = view_states_[&entry];

    if (entry.batches_dirty) {
        ctx.batch_builder->rebuild_batches(scene_state, vs.batches);
        if (camera) {
            prepend_environment_batch(*camera, vs, ctx);
        }
        entry.batches_dirty = false;
    }

    // RTT textures must exist + carry the right render_target_id BEFORE
    // build_draw_calls bakes those ids into draw data. Idempotent.
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

    ::velk::render::Frustum frustum;
    const ::velk::render::Frustum* frustum_ptr = nullptr;
    if (camera && vp_w > 0 && vp_h > 0) {
        frustum = ::velk::render::extract_frustum(vp_mat);
        frustum_ptr = &frustum;
    }

    float vp_x = has_viewport ? entry.viewport.x * sw : 0;
    float vp_y = has_viewport ? entry.viewport.y * sh : 0;
    emit_pass(entry, vs, ctx, globals_gpu_addr,
              {vp_x, vp_y, vp_w, vp_h}, frustum_ptr, out_passes);
}

void ForwardPath::emit_pass(ViewEntry& entry, ViewState& vs, FrameContext& ctx,
                            uint64_t globals_gpu_addr,
                            const rect& viewport,
                            const ::velk::render::Frustum* frustum,
                            vector<RenderPass>& out_passes)
{
    vector<DrawCall> draw_calls;
    ctx.batch_builder->build_draw_calls(vs.batches,
                                        draw_calls,
                                        *ctx.frame_buffer,
                                        *ctx.resources,
                                        globals_gpu_addr,
                                        ctx.pipeline_map,
                                        ctx.render_ctx,
                                        ctx.observer,
                                        frustum);

    RenderPass pass;
    pass.target.target = interface_pointer_cast<IRenderTarget>(entry.surface);
    pass.viewport = viewport;
    pass.draw_calls = std::move(draw_calls);
    out_passes.push_back(std::move(pass));
}

void ForwardPath::build_shared_passes(FrameContext& ctx, vector<RenderPass>& out_passes)
{
    if (ctx.render_target_cache) {
        ctx.render_target_cache->emit_passes(ctx, out_passes);
    }
}

void ForwardPath::on_view_removed(ViewEntry& view, FrameContext& /*ctx*/)
{
    view_states_.erase(&view);
}

void ForwardPath::shutdown(FrameContext& /*ctx*/)
{
    view_states_.clear();
}

} // namespace velk
