#ifndef VELK_UI_DEFERRED_LIGHTER_H
#define VELK_UI_DEFERRED_LIGHTER_H

#include <unordered_map>

#include "view_renderer.h"

namespace velk {

class DeferredGBufferPath;

/**
 * @brief Per-view compute pass that consumes the G-buffer and produces
 *        a fully shaded storage image.
 *
 * The DeferredGBufferPath emits a `PassKind::GBufferFill` that populates
 * the per-view G-buffer (albedo / normal / world-pos / material-params).
 * DeferredLighter runs a compute shader immediately after that pass,
 * sampling the G-buffer and writing per-pixel shaded color into a
 * storage image kept in the per-view state map. The result is then
 * blitted to the surface.
 *
 * Runs only for views whose camera uses `RenderPath::Deferred`.
 * Forward views stay on the plain raster path; RT views produce their
 * final image via the ray-tracer's compute+blit path.
 */
class DeferredLighter : public IViewRenderer
{
public:
    /// Set the path that owns G-buffer groups for deferred views. The
    /// lighter samples those groups; pointer must outlive the lighter.
    void set_gbuffer_source(DeferredGBufferPath* gbuffer) { gbuffer_source_ = gbuffer; }

    void build_passes(ViewEntry& view,
                      const SceneState& scene_state,
                      FrameContext& ctx,
                      vector<RenderPass>& out_passes) override;

    void on_view_removed(ViewEntry& view, FrameContext& ctx) override;
    void shutdown(FrameContext& ctx) override;

    /// Lookup for Renderer::get_shadow_debug_texture. Returns 0 if none
    /// is currently allocated for this view.
    TextureId find_shadow_debug_tex(ViewEntry* view) const;

private:
    struct ViewState
    {
        TextureId deferred_output_tex = 0;
        int deferred_width = 0;
        int deferred_height = 0;
        TextureId shadow_debug_tex = 0;
        int shadow_debug_width = 0;
        int shadow_debug_height = 0;
    };

    std::unordered_map<ViewEntry*, ViewState> view_states_;

    DeferredGBufferPath* gbuffer_source_ = nullptr;

    // Pipeline cache: different intersect-snippet sets compose to
    // different shader variants, each compiled once and kept across
    // frames. Key is an FNV-1a hash of the active intersect id list.
    std::unordered_map<uint64_t, bool> compiled_pipelines_;

    uint64_t ensure_pipeline(FrameContext& ctx);

    // Fullscreen composite pipeline that samples the deferred output
    // texture and alpha-blends it onto the surface. Compiled lazily on
    // first use; shared across views (surface render passes are compatible).
    uint64_t composite_pipeline_key_ = 0;
};

} // namespace velk

#endif // VELK_UI_DEFERRED_LIGHTER_H
