#ifndef VELK_SCENE_RENDER_PATH_VIEW_ENTRY_H
#define VELK_SCENE_RENDER_PATH_VIEW_ENTRY_H

#include <velk-render/interface/intf_window_surface.h>
#include <velk-render/render_types.h>

namespace velk {

/**
 * @brief Renderer-owned per-view identity (velk-render-pure).
 *
 * Carries surface + viewport + dirty / size cache. Scene-aware bits
 * (camera element, scene reference) are kept by the Renderer in a
 * parallel scene-side slot keyed by `ViewEntry*`; paths never need
 * them after Phase 3.3.
 *
 * `batches_dirty` is the cross-cutting flag the Renderer flips when
 * the scene's visual set changes; the scene-side preparer reads+clears
 * it before rebuilding batches.
 *
 * Stable address. Renderer never moves a `ViewEntry` for as long as
 * the view is registered, so paths can use `ViewEntry*` as a key in
 * their per-view state maps.
 */
struct ViewEntry
{
    IWindowSurface::Ptr surface;
    rect viewport;

    bool batches_dirty = true;
    int cached_width = 0;
    int cached_height = 0;
};

} // namespace velk

#endif // VELK_SCENE_RENDER_PATH_VIEW_ENTRY_H
