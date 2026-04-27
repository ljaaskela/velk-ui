#ifndef VELK_UI_RENDER_TARGET_CACHE_H
#define VELK_UI_RENDER_TARGET_CACHE_H

#include "view_renderer.h"

#include <unordered_map>
#include <velk-render/interface/intf_render_target.h>
#include <velk-render/render_types.h>
#include <velk-scene/interface/intf_element.h>

namespace velk {

class IElement;

/**
 * @brief Backing store for render-to-texture (RTT) subtrees.
 *
 * Lifetime sits on Renderer (one cache per renderer). Both raster paths
 * (Forward, Deferred G-buffer) call ensure() before building their main
 * draw passes so any TextureVisual sampling from an RTT sees a non-zero
 * texture id. ForwardPath's build_shared_passes drains emit_passes() to
 * actually render the RTT subtrees.
 */
class RenderTargetCache
{
public:
    /**
     * @brief Allocates / resizes the backend texture for every RTT
     *        subtree currently in the BatchBuilder's render_target_passes.
     *
     * Idempotent across frames. Must run before the per-view main
     * pass's build_draw_calls so the RTT's render_target_id is set on
     * its IRenderTarget object.
     */
    void ensure(FrameContext& ctx);

    /**
     * @brief Emits one Raster pass per RTT subtree into @p out_passes.
     *
     * Called from ForwardPath::build_shared_passes once per frame
     * after every view has finished its build_passes. The emitted
     * passes target the RTT textures, so they must run before any
     * surface pass that samples them — Renderer prepends shared
     * passes to the per-view passes for that reason.
     */
    void emit_passes(FrameContext& ctx, vector<RenderPass>& out_passes);

    /** @brief Releases the cached entry for @p elem, if any. */
    void on_element_removed(IElement* elem, FrameContext& ctx);

    /** @brief Destroys all cached textures during renderer shutdown. */
    void shutdown(FrameContext& ctx);

private:
    struct Entry
    {
        IRenderTarget::Ptr target;
        TextureId texture_id = 0;
        int width = 0;
        int height = 0;
        bool dirty = true;
    };

    std::unordered_map<IElement*, Entry> entries_;
};

} // namespace velk

#endif // VELK_UI_RENDER_TARGET_CACHE_H
