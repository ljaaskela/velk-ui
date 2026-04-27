#ifndef VELK_UI_RAY_TRACER_H
#define VELK_UI_RAY_TRACER_H

#include <velk/string.h>
#include <velk/vector.h>

#include <unordered_map>

#include "view_renderer.h"

namespace velk {
class IShadowTechnique;
} // namespace velk

namespace velk {

/**
 * @brief Per-view renderer that produces a compute-dispatch + blit pass.
 *
 * Owns the RT material registry (stable material ids + fill-snippet
 * metadata), the compiled compute pipeline cache (keyed by active material
 * set), and any per-view RT allocations (storage output textures live in
 * the per-view state map and are released in on_view_removed).
 *
 * Activated when a view's camera has render_path == RenderPath::RayTrace.
 * Skipped otherwise; no state is touched.
 */
class RayTracer : public IViewRenderer
{
public:
    void build_passes(ViewEntry& view,
                      const SceneState& scene_state,
                      FrameContext& ctx,
                      vector<RenderPass>& out_passes) override;
    void on_view_removed(ViewEntry& view, FrameContext& ctx) override;
    void shutdown(FrameContext& ctx) override;

private:
    struct ViewState
    {
        TextureId rt_output_tex = 0;
        int width = 0;
        int height = 0;
    };

    std::unordered_map<ViewEntry*, ViewState> view_states_;

    // Set of pipeline keys (hashes of material-id + shadow-tech-id sets)
    // we have compiled. Composition itself runs through the shared
    // FrameSnippetRegistry on ctx.
    std::unordered_map<uint64_t, bool> compiled_pipelines_;

    uint64_t ensure_pipeline(FrameContext& ctx);
};

} // namespace velk

#endif // VELK_UI_RAY_TRACER_H
