#ifndef VELK_RENDER_FRAME_RENDER_PASS_H
#define VELK_RENDER_FRAME_RENDER_PASS_H

#include <velk/vector.h>

#include <velk-render/interface/intf_render_target.h>
#include <velk-render/interface/intf_render_backend.h>
#include <velk-render/render_types.h>

namespace velk {

/** @brief Bundles a render target owned by a pass. */
struct ViewRenderTarget
{
    IRenderTarget::Ptr target;
};

/** @brief Discriminator for the kind of pass a render path produces. */
enum class PassKind
{
    Raster,       ///< Classic vkCmdBeginRenderPass + draw calls against a surface or RTT texture.
    ComputeBlit,  ///< Compute dispatch writing to a storage image, then blit to a surface.
    GBufferFill,  ///< Raster draws into a multi-attachment G-buffer group (no surface blit).
    Compute,      ///< Pure compute dispatch, no blit. Result consumed by a later pass via sampled image / storage.
    Blit,         ///< Pure blit from a source texture to a surface rect. Used for debug overlays.
};

/**
 * @brief A unit of GPU work produced by a render path and submitted by
 *        the Renderer in a single command buffer alongside other passes.
 *
 * Tagged union (see PassKind). Unused fields for the non-active kind
 * are zero-initialised.
 */
struct RenderPass
{
    PassKind kind = PassKind::Raster;

    ViewRenderTarget target;
    rect viewport;
    vector<DrawCall> draw_calls;

    /// GBufferFill kind: target group instead of `target`.
    RenderTargetGroup gbuffer_group = 0;

    /// ComputeBlit / Compute fields.
    DispatchCall compute{};
    uint64_t blit_surface_id = 0;
    TextureId blit_source = 0;
    rect blit_dst_rect{};

    /// Optional: after the color blit, copy this group's depth
    /// attachment into blit_surface_id's depth buffer. Zero = no depth
    /// blit.
    RenderTargetGroup blit_depth_source_group = 0;
};

} // namespace velk

#endif // VELK_RENDER_FRAME_RENDER_PASS_H
