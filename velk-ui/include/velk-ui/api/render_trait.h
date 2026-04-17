#ifndef VELK_UI_API_RENDER_TRAIT_H
#define VELK_UI_API_RENDER_TRAIT_H

#include <velk-render/api/render_technique.h>
#include <velk-ui/api/trait.h>

namespace velk::ui {

/**
 * @brief Base wrapper for traits that participate in rendering and host
 *        render-technique attachments.
 *
 * Scene-description traits (Trs, FixedSize, Stack, Visual, ...) inherit
 * Trait directly and do not carry techniques. RenderTrait extends Trait
 * for Camera, Light, and (future) Material-as-trait — anything that
 * configures how the renderer observes or contributes to a frame.
 *
 * Techniques attach to the trait they configure, not to the element
 * hosting the trait. Example:
 *
 *   auto light = trait::render::create_directional_light(dir, color);
 *   light.add_technique(create_rt_shadow());
 *   auto sun = ui::create_element();
 *   sun.add_trait(light);
 *
 * add_technique is a thin wrapper around IObjectStorage::add_attachment
 * — same mechanism as Element::add_trait, different named intent so
 * call sites distinguish scene description from render configuration.
 */
class RenderTrait : public Trait
{
public:
    RenderTrait() = default;

    explicit RenderTrait(IObject::Ptr obj) : Trait(std::move(obj)) {}
    explicit RenderTrait(ITrait::Ptr t) : Trait(std::move(t)) {}

    /**
     * @brief Attaches a render technique scoped to this trait.
     * @return Success on success, InvalidArgument if `tech` is empty.
     */
    ReturnValue add_technique(const RenderTechnique& tech)
    {
        return tech ? add_attachment(static_cast<IRenderTechnique::Ptr>(tech))
                    : ReturnValue::InvalidArgument;
    }

    /** @brief Removes a previously attached technique from this trait. */
    ReturnValue remove_technique(const RenderTechnique& tech)
    {
        return tech ? remove_attachment(static_cast<IRenderTechnique::Ptr>(tech))
                    : ReturnValue::InvalidArgument;
    }
};

} // namespace velk::ui

#endif // VELK_UI_API_RENDER_TRAIT_H
