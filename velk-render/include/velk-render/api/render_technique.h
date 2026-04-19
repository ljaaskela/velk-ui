#ifndef VELK_RENDER_API_RENDER_TECHNIQUE_H
#define VELK_RENDER_API_RENDER_TECHNIQUE_H

#include <velk/api/object.h>

#include <velk-render/interface/intf_render_technique.h>

namespace velk {

/**
 * @brief Generic wrapper around an IRenderTechnique object.
 *
 * Concrete sub-wrappers (ShadowTechnique, ReflectionTechnique, ...)
 * inherit this and add effect-specific accessors. Render-configuration
 * trait wrappers on the velk-ui side (RenderTrait::add_technique) take
 * values of this type.
 */
class RenderTechnique : public Object
{
public:
    RenderTechnique() = default;

    explicit RenderTechnique(IObject::Ptr obj) : Object(check_object<IRenderTechnique>(obj)) {}

    explicit RenderTechnique(IRenderTechnique::Ptr t) : Object(as_object(t)) {}

    operator IRenderTechnique::Ptr() const { return as_ptr<IRenderTechnique>(); }
};

} // namespace velk

#endif // VELK_RENDER_API_RENDER_TECHNIQUE_H
