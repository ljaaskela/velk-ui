#include "forward_path.h"

#include <velk-render/interface/material/intf_material.h>
#include <velk-render/interface/intf_render_target.h>
#include <velk-render/interface/intf_surface.h>

namespace velk {

void ForwardPath::build_passes(ViewEntry& entry,
                               const RenderView& render_view,
                               FrameContext& ctx,
                               vector<RenderPass>& out_passes)
{
    if (!ctx.backend || !ctx.frame_buffer || !ctx.material_cache || !ctx.pipeline_map) {
        return;
    }
    if (render_view.width <= 0 || render_view.height <= 0) return;

    const ::velk::render::Frustum* frustum_ptr =
        render_view.has_frustum ? &render_view.frustum : nullptr;
    auto& mcache = *ctx.material_cache;

    vector<DrawCall> draw_calls;

    // Env first (no frustum cull — fullscreen). Empty material =>
    // no env on this view's camera.
    if (render_view.env_batch.material) {
        vector<Batch> env_batches;
        env_batches.push_back(render_view.env_batch);
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

} // namespace velk
