#ifndef VELK_UI_RENDER_TO_TEXTURE_H
#define VELK_UI_RENDER_TO_TEXTURE_H

#include <velk-ui/ext/trait.h>
#include <velk-ui/interface/intf_render_to_texture.h>
#include <velk-ui/plugin.h>

namespace velk::ui::impl {

/**
 * @brief Trait that renders an element's subtree into a RenderTexture.
 *
 * Creates a RenderTexture on construction and exposes it via the
 * render_target ObjectRef property. The renderer detects this trait
 * and redirects the subtree's rendering into the texture.
 */
class RenderToTexture : public ::velk::ui::ext::Render<RenderToTexture, IRenderToTexture, IMetadataObserver>
{
public:
    VELK_CLASS_UID(::velk::ui::ClassId::Render::RenderToTexture, "RenderToTexture");

    RenderToTexture();

protected:
    void on_state_changed(string_view name, IMetadata& owner, Uid interfaceId) override;
};

} // namespace velk::ui::impl

#endif // VELK_UI_RENDER_TO_TEXTURE_H
