#ifndef VELK_RENDER_INTF_RENDER_TECHNIQUE_H
#define VELK_RENDER_INTF_RENDER_TECHNIQUE_H

#include <velk/interface/intf_metadata.h>
#include <velk/string_view.h>

#include <velk-render/interface/intf_render_context.h>

namespace velk {

/**
 * @brief Shader snippet + include filename for the render-technique composer.
 *
 * The renderer registers `snippet` under `include_name` so composed shaders
 * reference it via `#include "<include_name>"` and get readable line-number
 * diagnostics in compile errors. Matches the pattern IProgram already uses
 * for material fill snippets.
 */
struct ShaderInclude
{
    string_view snippet;       ///< Full GLSL body of the technique's evaluator.
    string_view include_name;  ///< Filename used for #include, e.g. "velk_shadow_rt.glsl".
};

/**
 * @brief Common base for pluggable rendering effects (shadows, reflections,
 *        AO, GI, ...).
 *
 * Every technique contributes a GLSL snippet that declares an evaluator
 * function. The lighting / post composer stitches the snippet in as a
 * shader include, generates a switch keyed on technique class UIDs, and
 * dispatches to `get_fn_name()` for each active technique. This is the
 * same machinery `ray_tracer.cpp::ensure_pipeline` already uses for
 * material fill snippets; sub-interfaces (IShadowTechnique, ...) pin
 * down the exact GLSL function signature and declare a prepare()
 * context suited to the effect.
 *
 * Techniques attach to the trait they configure (shadow on Light,
 * reflection/AO/GI/post on Camera) via the normal velk attachment
 * mechanism (`IObjectStorage::add_attachment`). They do not participate
 * in any UI trait machinery.
 */
class IRenderTechnique : public Interface<IRenderTechnique>
{
public:
    /**
     * @brief Returns the GLSL snippet defining this technique's evaluator,
     *        plus the include filename under which it should be registered.
     *
     * The snippet is self-contained: it declares any buffer_reference
     * structs, sampler2D resources, and helper functions it needs, plus
     * exactly one entry function named by `get_fn_name()`. The function
     * signature is fixed per sub-interface.
     */
    virtual ShaderInclude get_shader_include() const = 0;

    /**
     * @brief Returns the entry-point function name defined in the snippet.
     *
     * Must be unique across techniques the composer might see together.
     * Typically derived from the class name, e.g. "velk_shadow_rt".
     */
    virtual string_view get_fn_name() const = 0;

    /**
     * @brief Hook for techniques whose snippet depends on additional
     *        shader includes.
     *
     * The composer calls this once when the technique class is first
     * encountered, before compiling any pipeline that references it.
     * Implementations call `ctx.register_shader_include(name, src)` for
     * each dependency. Idempotent. Default empty for techniques with
     * no transitive includes.
     */
    virtual void register_includes(IRenderContext& /*ctx*/) const {}
};

} // namespace velk

#endif // VELK_RENDER_INTF_RENDER_TECHNIQUE_H
