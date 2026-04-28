#ifndef VELK_RENDER_CAMERA_PIPELINE_H
#define VELK_RENDER_CAMERA_PIPELINE_H

#include <velk-render/plugin.h>
#include <velk-render/ext/view_pipeline.h>
#include <velk-render/render_path/intf_render_path.h>
#include <velk-render/render_path/intf_view_pipeline.h>

namespace velk::impl {

/**
 * @brief Default per-camera view pipeline.
 *
 * Auto-attached to every Camera trait at construction time. Composes
 * the IRenderPath also attached to the camera trait; falls back to a
 * built-in ForwardPath when no path is attached.
 *
 * Holds the built-in fallback path so the Renderer doesn't have to.
 * `needs()` forwards from the resolved path so ViewPreparer extracts
 * only the collections the path actually consumes.
 *
 * Lifetime hooks (`on_view_removed`, `shutdown`) forward to the path
 * the pipeline last dispatched against; if a different path gets
 * attached later, the previous path's per-view state is reclaimed when
 * the Renderer drives `seen_pipelines_` teardown.
 */
class CameraPipeline final
    : public ::velk::ext::ViewPipeline<CameraPipeline>
{
public:
    VELK_CLASS_UID(::velk::ClassId::Pipeline::Camera, "CameraPipeline");

    CameraPipeline();

    ::velk::IRenderPath::Needs needs(const ::velk::FrameContext& ctx) const override;

    void emit(::velk::ViewEntry& view,
              const ::velk::RenderView& render_view,
              ::velk::IRenderTarget::Ptr color_target,
              ::velk::FrameContext& ctx,
              ::velk::IRenderGraph& graph) override;

    void on_view_removed(::velk::ViewEntry& view, ::velk::FrameContext& ctx) override;
    void shutdown(::velk::FrameContext& ctx) override;

private:
    /// Pipeline can't reach the camera trait it's attached to; the
    /// Renderer passes the trait pointer in `FrameContext` (see
    /// `FrameContext::view_camera_trait`). Falls back to the built-in
    /// forward path when no IRenderPath is attached.
    ::velk::IRenderPath* resolve_path(const ::velk::FrameContext& ctx) const;

    ::velk::IRenderPath::Ptr fallback_path_;
};

} // namespace velk::impl

#endif // VELK_RENDER_CAMERA_PIPELINE_H
