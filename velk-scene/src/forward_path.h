#ifndef VELK_UI_FORWARD_PATH_H
#define VELK_UI_FORWARD_PATH_H

#include <velk/ext/core_object.h>
#include <velk/vector.h>

#include <unordered_map>

#include <velk-render/frustum.h>
#include <velk-render/interface/intf_camera.h>
#include <velk-scene/plugin.h>
#include <velk-scene/render_path/frame_context.h>
#include <velk-scene/render_path/intf_render_path.h>
#include <velk-scene/render_path/view_entry.h>

#include "batch_builder.h"

namespace velk {

/**
 * @brief Classic forward shading path.
 *
 * Activated when no path is attached (built-in fallback) or when a
 * camera explicitly attaches a `ForwardPath`. Emits one Raster pass per
 * view with the env batch (prepended in batch rebuild) followed by
 * scene geometry. RTT subtrees are emitted from `build_shared_passes`
 * via the shared `RenderTargetCache` on `FrameContext`.
 */
class ForwardPath : public ext::ObjectCore<ForwardPath, IRenderPath>
{
public:
    VELK_CLASS_UID(ClassId::Path::Forward, "ForwardPath");

    void build_passes(ViewEntry& view,
                      const SceneState& scene_state,
                      FrameContext& ctx,
                      vector<RenderPass>& out_passes) override;

    void build_shared_passes(FrameContext& ctx,
                             vector<RenderPass>& out_passes) override;

    void on_view_removed(ViewEntry& view, FrameContext& ctx) override;
    void shutdown(FrameContext& ctx) override;

private:
    struct ViewState
    {
        vector<BatchBuilder::Batch> batches;
    };

    std::unordered_map<ViewEntry*, ViewState> view_states_;

    void prepend_environment_batch(ICamera& camera, ViewState& vs, FrameContext& ctx);
    void emit_pass(ViewEntry& view, ViewState& vs, FrameContext& ctx,
                   uint64_t globals_gpu_addr,
                   const rect& viewport,
                   const ::velk::render::Frustum* frustum,
                   vector<RenderPass>& out_passes);
};

} // namespace velk

#endif // VELK_UI_FORWARD_PATH_H
