#ifndef VELK_UI_DEFERRED_GBUFFER_PATH_H
#define VELK_UI_DEFERRED_GBUFFER_PATH_H

#include "view_renderer.h"

#include <unordered_map>
#include <velk-render/frustum.h>

namespace velk {

/**
 * @brief Per-view renderer that emits the deferred G-buffer fill pass.
 *
 * Activated for views whose camera has render_path == RenderPath::Deferred.
 * Allocates the per-view G-buffer (multi-attachment render target group)
 * and emits a single GBufferFill pass; DeferredLighter consumes the
 * attachments in a subsequent compute pass.
 */
class DeferredGBufferPath : public IViewRenderer
{
public:
    void build_passes(ViewEntry& view,
                      const SceneState& scene_state,
                      FrameContext& ctx,
                      vector<RenderPass>& out_passes) override;

    void on_view_removed(ViewEntry& view, FrameContext& ctx) override;
    void shutdown(FrameContext& ctx) override;

    /// Lookup for Renderer::get_gbuffer_attachment.
    /// Returns 0 if no G-buffer has been allocated for this view yet.
    RenderTargetGroup find_gbuffer_group(ViewEntry* view) const;

    int find_gbuffer_width(ViewEntry* view) const;
    int find_gbuffer_height(ViewEntry* view) const;

private:
    struct ViewState
    {
        RenderTargetGroup gbuffer_group = 0;
        int width = 0;
        int height = 0;
    };

    std::unordered_map<ViewEntry*, ViewState> view_states_;

    RenderTargetGroup ensure_gbuffer(ViewEntry& view, ViewState& vs,
                                     int width, int height, FrameContext& ctx);

    void emit_pass(ViewEntry& view, ViewState& vs, FrameContext& ctx,
                   uint64_t globals_gpu_addr,
                   const ::velk::render::Frustum* frustum,
                   vector<RenderPass>& out_passes);
};

} // namespace velk

#endif // VELK_UI_DEFERRED_GBUFFER_PATH_H
