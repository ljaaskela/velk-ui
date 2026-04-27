#include "forward_path.h"

#include "env_helper.h"
#include "render_target_cache.h"

#include <velk/api/perf.h>
#include <velk/api/state.h>
#include <velk/interface/intf_object_storage.h>

#include <velk-render/interface/material/intf_material.h>
#include <velk-render/interface/intf_surface.h>
#include <velk-scene/interface/intf_environment.h>

namespace velk {

namespace {

/// Builds the forward-only env batch (fullscreen quad with the camera's
/// environment material) for prepending to the main draw list. Empty
/// vector if the camera has no environment or no material.
vector<Batch> build_env_batches(ViewEntry& entry, FrameContext& ctx)
{
    vector<Batch> out;
    auto camera = ::velk::find_attachment<ICamera>(entry.camera_element);
    if (!camera || !ctx.render_ctx) return out;

    auto resolved = ensure_env_ready(*camera, ctx);
    if (!resolved.env || !resolved.surface) return out;

    auto material = resolved.env->get_material();
    if (!material) return out;

    Batch env_batch;
    env_batch.pipeline_key = 0; // material override supplies the pipeline
    env_batch.texture_key = reinterpret_cast<uint64_t>(resolved.surface);
    env_batch.instance_stride = 4;
    env_batch.instance_count = 1;
    env_batch.instance_data.resize(4, 0);
    env_batch.material = std::move(material);
    auto quad = ctx.render_ctx->get_mesh_builder().get_unit_quad();
    if (quad) {
        auto prims = quad->get_primitives();
        if (prims.size() > 0) env_batch.primitive = prims[0];
    }
    out.push_back(std::move(env_batch));
    return out;
}

} // namespace

void ForwardPath::build_passes(ViewEntry& entry,
                               const SceneState& /*scene_state*/,
                               const RenderView& render_view,
                               FrameContext& ctx,
                               vector<RenderPass>& out_passes)
{
    if (!ctx.backend || !ctx.frame_buffer || !ctx.batch_builder || !ctx.pipeline_map) {
        return;
    }
    if (render_view.width <= 0 || render_view.height <= 0) return;

    // RTT textures must exist + carry the right render_target_id BEFORE
    // build_draw_calls bakes those ids into draw data. Idempotent.
    if (ctx.render_target_cache) {
        ctx.render_target_cache->ensure(ctx);
    }

    const ::velk::render::Frustum* frustum_ptr =
        render_view.has_frustum ? &render_view.frustum : nullptr;
    auto& mcache = ctx.batch_builder->material_addr_cache();

    vector<DrawCall> draw_calls;

    // Env first (no frustum cull — fullscreen).
    auto env_batches = build_env_batches(entry, ctx);
    if (!env_batches.empty()) {
        auto env_draws = ctx.render_ctx->build_draw_calls(
            env_batches, *ctx.frame_buffer, *ctx.resources,
            render_view.frame_globals_addr, ctx.observer, mcache,
            /*frustum=*/nullptr);
        for (auto& dc : env_draws) {
            draw_calls.push_back(std::move(dc));
        }
    }

    // Main scene batches.
    if (render_view.batches && !render_view.batches->empty()) {
        auto main_draws = ctx.render_ctx->build_draw_calls(
            *render_view.batches, *ctx.frame_buffer, *ctx.resources,
            render_view.frame_globals_addr, ctx.observer, mcache, frustum_ptr);
        for (auto& dc : main_draws) {
            draw_calls.push_back(std::move(dc));
        }
    }

    RenderPass pass;
    pass.target.target = interface_pointer_cast<IRenderTarget>(entry.surface);
    pass.viewport = render_view.viewport;
    pass.draw_calls = std::move(draw_calls);
    out_passes.push_back(std::move(pass));
}

void ForwardPath::build_shared_passes(FrameContext& ctx, vector<RenderPass>& out_passes)
{
    if (ctx.render_target_cache) {
        ctx.render_target_cache->emit_passes(ctx, out_passes);
    }
}

} // namespace velk
