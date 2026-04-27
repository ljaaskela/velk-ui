#ifndef VELK_UI_FORWARD_PATH_H
#define VELK_UI_FORWARD_PATH_H

#include <velk/ext/core_object.h>
#include <velk/vector.h>

#include <unordered_map>

#include <velk-render/frame/batch.h>
#include <velk-render/frustum.h>
#include <velk-render/plugin.h>
#include <velk-render/render_path/frame_context.h>
#include <velk-render/render_path/intf_render_path.h>
#include <velk-render/render_path/view_entry.h>

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

    Needs needs() const override
    {
        Needs n;
        n.batches = true;
        return n;
    }

    void build_passes(ViewEntry& view,
                      const RenderView& render_view,
                      FrameContext& ctx,
                      vector<RenderPass>& out_passes) override;
};

} // namespace velk

#endif // VELK_UI_FORWARD_PATH_H
