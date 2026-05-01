#ifndef VELK_RENDER_RT_SHADOW_H
#define VELK_RENDER_RT_SHADOW_H

#include <velk/ext/object.h>

#include <velk-render/interface/intf_shader_source.h>
#include <velk-render/interface/intf_shadow_technique.h>
#include <velk-render/plugin.h>

namespace velk::impl {

/**
 * @brief Ray-traced shadow technique: traces one occlusion ray against
 *        the shared scene shape buffer.
 *
 * Multi-implements `IShadowTechnique` (the technique tag) and
 * `IShaderSource` (the GLSL snippet under role `shader_role::kShadow`).
 */
class RtShadow : public ::velk::ext::Object<RtShadow, IShadowTechnique, IShaderSource>
{
public:
    VELK_CLASS_UID(ClassId::RtShadow, "RtShadow");

    // IRenderTechnique
    string_view get_technique_name() const override { return "rt_shadow"; }

    // IShaderSource
    string_view get_source(string_view role) const override;
    string_view get_fn_name(string_view role) const override;
    uint64_t get_pipeline_key() const override { return 0; }
    void register_includes(IRenderContext&) const override {}

    // IShadowTechnique
    void prepare(ShadowContext& /*ctx*/) override {}
};

} // namespace velk::impl

#endif // VELK_RENDER_RT_SHADOW_H
