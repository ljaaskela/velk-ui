#ifndef VELK_SCENE_INTF_RENDER_PATH_H
#define VELK_SCENE_INTF_RENDER_PATH_H

#include <velk/interface/intf_interface.h>
#include <velk/uid.h>
#include <velk/vector.h>

#include <velk-render/frame/render_pass.h>
#include <velk-scene/interface/intf_scene_observer.h>
#include <velk-scene/render_path/frame_context.h>
#include <velk-scene/render_path/view_entry.h>

namespace velk {

/**
 * @brief Per-view rendering strategy attached to a camera element.
 *
 * Implementations turn a single view (camera + surface + viewport +
 * scene state) into zero or more `RenderPass` records that the renderer
 * submits to the backend. Built-in paths cover Forward, Deferred, and
 * RT (compute-shader path tracer); plugins can register additional
 * paths and attach them to camera elements like any other trait.
 *
 * Lifetime: owned by the camera element via `add_attachment`. The
 * Renderer discovers the path via `find_attachment<IRenderPath>` per
 * view per frame; no attachment falls back to a built-in `Forward`
 * path so trivial UI samples don't have to opt in explicitly.
 *
 * Per-view state: implementations typically keep an
 * `unordered_map<ViewEntry*, ViewState>` keyed by the stable
 * `ViewEntry*`. The renderer calls `on_view_removed` so paths can
 * release per-view GPU resources, and `shutdown` so paths can release
 * everything during renderer teardown.
 */
class IRenderPath
    : public Interface<IRenderPath, IInterface,
                       VELK_UID("15116d0d-6a52-40b4-94f9-f5ab9ffc133f")>
{
public:
    /**
     * @brief Appends zero or more passes for @p view to @p out_passes.
     *
     * Called once per frame per view by the renderer. The implementation
     * may maintain per-view state keyed off `&view`.
     */
    virtual void build_passes(ViewEntry& view,
                              const SceneState& scene_state,
                              FrameContext& ctx,
                              vector<RenderPass>& out_passes) = 0;

    /**
     * @brief Emits frame-scope passes that are not per-view (e.g.
     *        raster render-to-texture passes that serve multiple views).
     *
     * Called once per frame after every `build_passes` call so the path
     * can observe the full per-frame batch state before emitting.
     * Default no-op.
     */
    virtual void build_shared_passes(FrameContext& /*ctx*/,
                                     vector<RenderPass>& /*out_passes*/) {}

    /** @brief Hook called when a view is removed. Release per-view state. */
    virtual void on_view_removed(ViewEntry& /*view*/, FrameContext& /*ctx*/) {}

    /**
     * @brief Hook called when an element is detached from a scene.
     *
     * Fires once per detached element during the per-frame consume pass
     * before any `build_passes` call. Implementations release any
     * per-element state they cache (e.g. RTT textures keyed by element).
     * Default no-op.
     */
    virtual void on_element_removed(IElement* /*elem*/, FrameContext& /*ctx*/) {}

    /** @brief Hook called on Renderer shutdown. Release all remaining state. */
    virtual void shutdown(FrameContext& /*ctx*/) {}
};

} // namespace velk

#endif // VELK_SCENE_INTF_RENDER_PATH_H
