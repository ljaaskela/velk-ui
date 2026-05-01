#ifndef VELK_UI_INTF_RENDER_TO_TEXTURE_H
#define VELK_UI_INTF_RENDER_TO_TEXTURE_H

#include <velk/api/math_types.h>
#include <velk/interface/intf_metadata.h>

#include <velk-scene/interface/intf_trait.h>

namespace velk {

/**
 * @brief Trait interface for rendering an element's subtree into a texture.
 *
 * The user supplies a `RenderTexture` instance via @c render_target — a
 * named identity that wires the producer (this trait) to consumers
 * elsewhere (e.g. a `TextureVisual` sampling the same instance). The
 * wrapper carries no GPU storage at construction; the renderer's
 * `RenderTargetCache` allocates a backend texture into it on first
 * capture, sized from the element's layout (or `texture_size` override).
 * The wrapper's pixel format determines the captured format.
 */
class IRenderToTexture : public Interface<IRenderToTexture, ITrait>
{
public:
    VELK_INTERFACE(
        (PROP, uvec2, texture_size, {}),     ///< Texture dimensions. (0,0) = match element size.
        (PROP, ObjectRef, render_target, {}) ///< User-supplied RenderTexture. Cache fills in its backend handle on first capture.
    )
};

} // namespace velk

#endif // VELK_UI_INTF_RENDER_TO_TEXTURE_H
