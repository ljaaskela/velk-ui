#include "camera_pipeline.h"

#include "forward_path.h"

#include <velk/api/velk.h>
#include <velk/interface/intf_object_storage.h>

namespace velk::impl {

CameraPipeline::CameraPipeline()
    : fallback_path_(::velk::instance().create<::velk::IRenderPath>(
          ::velk::ClassId::Path::Forward))
{
}

::velk::IRenderPath* CameraPipeline::resolve_path(const ::velk::FrameContext& ctx) const
{
    if (ctx.view_camera_trait) {
        if (auto p = ::velk::find_attachment<::velk::IRenderPath>(ctx.view_camera_trait)) {
            return p.get();
        }
    }
    return fallback_path_.get();
}

::velk::IRenderPath::Needs CameraPipeline::needs(const ::velk::FrameContext& ctx) const
{
    if (auto* path = resolve_path(ctx)) {
        return path->needs();
    }
    return {};
}

void CameraPipeline::emit(::velk::ViewEntry& view,
                          const ::velk::RenderView& render_view,
                          ::velk::IRenderTarget::Ptr color_target,
                          ::velk::FrameContext& ctx,
                          ::velk::IRenderGraph& graph)
{
    if (auto* path = resolve_path(ctx)) {
        path->build_passes(view, render_view, color_target, ctx, graph);
    }
}

void CameraPipeline::on_view_removed(::velk::ViewEntry& view, ::velk::FrameContext& ctx)
{
    if (auto* path = resolve_path(ctx)) {
        path->on_view_removed(view, ctx);
    }
}

void CameraPipeline::shutdown(::velk::FrameContext& ctx)
{
    if (fallback_path_) {
        fallback_path_->shutdown(ctx);
    }
    // Attached paths are owned by the camera trait and shut down when
    // the trait drops them; the pipeline only owns its fallback.
}

} // namespace velk::impl
