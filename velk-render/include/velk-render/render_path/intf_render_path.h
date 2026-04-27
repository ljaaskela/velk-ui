#ifndef VELK_SCENE_INTF_RENDER_PATH_H
#define VELK_SCENE_INTF_RENDER_PATH_H

#include <velk/interface/intf_interface.h>
#include <velk/uid.h>
#include <velk/vector.h>

#include <velk-render/frame/render_pass.h>
#include <velk-render/frame/render_view.h>
#include <velk-render/render_path/frame_context.h>
#include <velk-render/render_path/view_entry.h>

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
     * @brief Per-path declaration of which scene-side data the
     *        preparer should collect into the path's `RenderView`.
     *
     * Static: returned by `needs()` and queried once per dispatch.
     * Defaults to "nothing" — concrete paths opt into the collections
     * they actually consume so trivial UI scenes don't pay for RT
     * shape walks they'll never use.
     */
    struct Needs
    {
        bool batches = false; ///< Raster batch list (Forward + Deferred).
        bool shapes  = false; ///< RT primary-buffer shapes (RT).
        bool lights  = false; ///< Scene lights + shadow-tech registration (Deferred + RT).
    };

    /// What this path consumes from `RenderView`. ViewPreparer reads this
    /// once per dispatch and skips collections the path doesn't want.
    virtual Needs needs() const { return {}; }

    /**
     * @brief Appends zero or more passes for @p view to @p out_passes.
     *
     * Called once per frame per view by the renderer. The implementation
     * may maintain per-view state keyed off `&view`. @p render_view is
     * a flat snapshot of resolved scene data for this view (camera
     * matrices, lights, env, raster batches, BVH addresses) — paths
     * consume this exclusively; no scene-graph reach-back.
     */
    virtual void build_passes(ViewEntry& view,
                              const RenderView& render_view,
                              FrameContext& ctx,
                              vector<RenderPass>& out_passes) = 0;

    /** @brief Hook called when a view is removed. Release per-view state. */
    virtual void on_view_removed(ViewEntry& /*view*/, FrameContext& /*ctx*/) {}

    /** @brief Hook called on Renderer shutdown. Release all remaining state. */
    virtual void shutdown(FrameContext& /*ctx*/) {}

    /// Optional debug accessor: returns the per-view G-buffer render
    /// target group if this path maintains one. Default 0 (no G-buffer).
    /// Used by `Renderer::get_gbuffer_attachment` for debug-overlay
    /// readback; non-deferred paths leave the default.
    virtual RenderTargetGroup find_gbuffer_group(ViewEntry* /*view*/) const { return 0; }

    /// Optional debug accessor: returns the per-view RT-shadow debug
    /// storage texture if this path maintains one. Default 0.
    virtual TextureId find_shadow_debug_tex(ViewEntry* /*view*/) const { return 0; }
};

} // namespace velk

#endif // VELK_SCENE_INTF_RENDER_PATH_H
