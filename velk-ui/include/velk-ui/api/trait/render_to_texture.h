#ifndef VELK_UI_API_TRAIT_RENDER_TO_TEXTURE_H
#define VELK_UI_API_TRAIT_RENDER_TO_TEXTURE_H

#include <velk/api/state.h>
#include <velk/api/velk.h>
#include <velk-ui/api/element.h>
#include <velk-ui/interface/intf_render_to_texture.h>
#include <velk-ui/plugin.h>

namespace velk::ui {

/**
 * @brief Convenience wrapper for a RenderToTexture trait.
 *
 *   auto rtt = trait::render::create_render_to_texture();
 *   element.add_trait(rtt);
 *
 *   // Use the texture on another element
 *   auto state = read_state<IRenderToTexture>(rtt.ptr());
 *   auto tex = state->render_target.get<ITexture>();
 */
class RenderToTextureTrait : public Trait
{
public:
    RenderToTextureTrait() = default;
    explicit RenderToTextureTrait(IObject::Ptr obj) : Trait(std::move(obj)) {}

    /** @brief Sets the texture resolution. (0,0) matches the element's layout size. */
    void set_texture_size(uint32_t w, uint32_t h)
    {
        write_state<IRenderToTexture>(ptr(), [&](IRenderToTexture::State& s) {
            s.texture_size = {w, h};
        });
    }

    /** @brief Returns the render target (RenderTexture) for binding to a TextureVisual. */
    IObject::Ptr get_render_target() const
    {
        auto state = read_state<IRenderToTexture>(ptr());
        return state ? state->render_target.get() : nullptr;
    }
};

namespace trait::render {

/** @brief Creates a new RenderToTexture trait. */
inline RenderToTextureTrait create_render_to_texture()
{
    return RenderToTextureTrait(instance().create<IObject>(ClassId::Render::RenderToTexture));
}

} // namespace trait::render

} // namespace velk::ui

#endif // VELK_UI_API_TRAIT_RENDER_TO_TEXTURE_H
