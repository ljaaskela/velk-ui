#ifndef VELK_UI_FORWARD_PATH_H
#define VELK_UI_FORWARD_PATH_H

#include "view_renderer.h"

#include <velk-render/frustum.h>
#include <velk-render/interface/intf_camera.h>

namespace velk {

/**
 * @brief Per-view renderer for the classic forward-shading path.
 *
 * Activated for views whose camera has render_path == RenderPath::Forward
 * (or no camera attached). Emits one Raster pass per view: env batch
 * (prepended in batch rebuild) renders first, then scene geometry. RTT
 * subtrees are emitted from build_shared_passes via the shared
 * RenderTargetCache on FrameContext.
 */
class ForwardPath : public IViewRenderer
{
public:
    void build_passes(ViewEntry& view,
                      const SceneState& scene_state,
                      FrameContext& ctx,
                      vector<RenderPass>& out_passes) override;

    void build_shared_passes(FrameContext& ctx,
                             vector<RenderPass>& out_passes) override;

private:
    void prepend_environment_batch(ICamera& camera, ViewEntry& view, FrameContext& ctx);
    void emit_pass(ViewEntry& view, FrameContext& ctx,
                   uint64_t globals_gpu_addr,
                   const rect& viewport,
                   const ::velk::render::Frustum* frustum,
                   vector<RenderPass>& out_passes);
};

} // namespace velk

#endif // VELK_UI_FORWARD_PATH_H
