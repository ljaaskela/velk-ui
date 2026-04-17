#ifndef VELK_UI_TRAIT_LIGHT_H
#define VELK_UI_TRAIT_LIGHT_H

#include <velk-ui/ext/trait.h>
#include <velk-ui/interface/intf_light.h>
#include <velk-ui/plugin.h>

namespace velk::ui::impl {

/**
 * @brief Default light trait implementation.
 *
 * Pure data container — properties via ILight's state macro. The
 * renderer reads the owning element's world_matrix for position and
 * forward axis; this class holds only the intrinsic source properties
 * (type, colour, intensity, range, cone angles). Shadow casting is
 * delegated to an IShadowTechnique attached via RenderTrait::add_technique.
 */
class Light : public ext::Render<Light, ILight>
{
public:
    VELK_CLASS_UID(ClassId::Render::Light, "Light");
};

} // namespace velk::ui::impl

#endif // VELK_UI_TRAIT_LIGHT_H
