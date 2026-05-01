#ifndef VELK_RENDER_INTF_RENDER_TECHNIQUE_H
#define VELK_RENDER_INTF_RENDER_TECHNIQUE_H

#include <velk/interface/intf_metadata.h>
#include <velk/string_view.h>

namespace velk {

/**
 * @brief Tag base for pluggable rendering effects (shadows, reflections,
 *        AO, GI, post-processing).
 *
 * Pure tag interface today; sub-interfaces (`IShadowTechnique`,
 * `IReflectionTechnique`, ...) pin down the per-effect prepare()
 * context and the GLSL function signature their snippet must satisfy.
 *
 * Techniques *also* implement `IShaderSource` to contribute their GLSL
 * snippet (under role `shader_role::kShadow` for shadow techniques,
 * etc.) — implementations multi-implement both interfaces. The
 * inheritance is intentionally separate so technique kinds whose
 * contribution is optional or non-shader-based aren't forced into
 * IShaderSource.
 *
 * Techniques attach to the render-trait they configure (shadow on
 * Light, reflection / AO / GI / post on Camera) via the normal velk
 * attachment mechanism.
 */
class IRenderTechnique : public Interface<IRenderTechnique>
{
public:
    /// Short, human-readable name for logs / debug overlays.
    /// E.g. "rt_shadow", "shadow_map", "ssao", "rt_reflect".
    virtual string_view get_technique_name() const = 0;
};

} // namespace velk

#endif // VELK_RENDER_INTF_RENDER_TECHNIQUE_H
