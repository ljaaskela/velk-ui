#ifndef VELK_UI_VIEW_PREPARER_H
#define VELK_UI_VIEW_PREPARER_H

#include <velk/vector.h>

#include <unordered_map>

#include <velk-render/frame/batch.h>
#include <velk-render/frame/render_view.h>
#include <velk-scene/interface/intf_scene_observer.h>
#include <velk-scene/render_path/frame_context.h>
#include <velk-scene/render_path/view_entry.h>

namespace velk {

/**
 * @brief Walks a `ViewEntry` + scene state and produces a flat `RenderView`.
 *
 * Centralises everything a path used to do for itself before emitting
 * GPU passes:
 *   - Resolves the camera into view/inverse_view-projection matrices
 *     plus world position.
 *   - Computes pixel viewport from surface size + viewport rect.
 *   - Writes `FrameGlobals` to the frame buffer; returns its address.
 *   - Rebuilds the raster batch cache when `entry.batches_dirty` is
 *     set (cache lifetime is per-view, persistent across frames).
 *   - Collects scene lights into `RenderView::lights`.
 *   - Resolves the camera's environment into `RenderView::env`.
 *
 * RT shape collection still lives on `RtPath` for now; migrates in 3.3.
 *
 * Lifetime: the per-view batch cache is keyed by `ViewEntry*`.
 * `on_view_removed` releases it; `clear()` releases all (called on
 * Renderer shutdown).
 */
class ViewPreparer
{
public:
    /// Builds a `RenderView` for @p entry. The returned RenderView's
    /// `batches` pointer aliases the preparer-owned cache for this
    /// view; valid until the next `prepare` for the same view or
    /// until `on_view_removed` / `clear`.
    RenderView prepare(ViewEntry& entry,
                       const SceneState& scene_state,
                       FrameContext& ctx);

    /// Releases the per-view batch cache for @p entry.
    void on_view_removed(ViewEntry& entry);

    /// Releases all per-view caches. Called on Renderer shutdown.
    void clear();

private:
    struct ViewCache
    {
        vector<Batch> batches;
    };
    std::unordered_map<ViewEntry*, ViewCache> view_caches_;
};

} // namespace velk

#endif // VELK_UI_VIEW_PREPARER_H
