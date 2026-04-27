#ifndef VELK_SCENE_RENDER_PATH_VIEW_ENTRY_H
#define VELK_SCENE_RENDER_PATH_VIEW_ENTRY_H

#include <velk-render/interface/intf_window_surface.h>
#include <velk-render/render_types.h>
#include <velk-scene/interface/intf_element.h>

namespace velk {

/**
 * @brief Renderer-owned per-view identity + cached size.
 *
 * Carries only state that is path-agnostic: which camera, which surface,
 * and the viewport rect within that surface. Path-specific resources
 * (RT output texture, G-buffer group, deferred output, batch caches)
 * live in per-path `ViewEntry* -> state` maps owned by each render
 * path.
 *
 * `batches_dirty` is the one cross-cutting flag: the renderer flips it
 * when the scene's visual set changes; raster paths read+clear it on
 * the next `build_passes` call to know they need to rebuild their
 * cached batches.
 *
 * Stable address. Renderer never moves a `ViewEntry` for as long as
 * the view is registered, so paths can use `ViewEntry*` as a key in
 * their per-view state maps.
 */
struct ViewEntry
{
    IElement::Ptr camera_element;
    IWindowSurface::Ptr surface;
    rect viewport;

    bool batches_dirty = true;
    int cached_width = 0;
    int cached_height = 0;
};

} // namespace velk

#endif // VELK_SCENE_RENDER_PATH_VIEW_ENTRY_H
