#ifndef VELK_RENDER_POST_PROCESS_H
#define VELK_RENDER_POST_PROCESS_H

#include <velk/vector.h>

#include <unordered_map>

#include <velk-render/ext/post_process.h>
#include <velk-render/interface/intf_effect.h>
#include <velk-render/interface/intf_render_target.h>
#include <velk-render/plugin.h>

namespace velk::impl {

/**
 * @brief Linear post-process container.
 *
 * The default `IPostProcess` implementation. Composes attached
 * `IEffect` children in attachment order via per-view intermediate
 * textures. Discovery: `find_attachments<IEffect>(this)` returns the
 * effects; the container runs them sequentially, last child writing
 * directly to the container's `output`.
 *
 * Per-view state (intermediates) keyed off `ViewEntry*`, so one
 * container Ptr can be attached to multiple pipelines / cameras and
 * serve all of them safely. `on_view_removed` releases just that
 * view's slot; `shutdown` clears everything.
 *
 * The container's caller (CameraPipeline or another container)
 * guarantees `output` is a storage-writable texture, so effects
 * never need to special-case "am I writing to a surface?".
 *
 * Tier 1: one intermediate texture per non-last child. Tier 2
 * transient pool will alias these.
 */
class PostProcess final
    : public ::velk::ext::PostProcess<PostProcess>
{
public:
    VELK_CLASS_UID(::velk::ClassId::Post::PostProcess, "PostProcess");

    void emit(::velk::ViewEntry& view,
              ::velk::IRenderTarget::Ptr input,
              ::velk::IRenderTarget::Ptr output,
              ::velk::FrameContext& ctx,
              ::velk::IRenderGraph& graph) override;

    void on_view_removed(::velk::ViewEntry& view, ::velk::FrameContext& ctx) override;
    void shutdown(::velk::FrameContext& ctx) override;

private:
    struct ViewState
    {
        /// One intermediate per non-last effect (last effect writes
        /// the output target the container was given). Allocated
        /// lazily, resized when input dimensions change.
        ::velk::vector<::velk::IRenderTarget::Ptr> intermediates;
        int width = 0;
        int height = 0;
    };

    std::unordered_map<::velk::ViewEntry*, ViewState> view_states_;

    ::velk::IRenderTarget::Ptr ensure_intermediate(::velk::ViewEntry& view,
                                                   size_t index,
                                                   int width, int height,
                                                   ::velk::FrameContext& ctx,
                                                   ::velk::IRenderGraph& graph);

    void release_view_state(ViewState& vs, ::velk::FrameContext& ctx);
};

} // namespace velk::impl

#endif // VELK_RENDER_POST_PROCESS_H
