#ifndef VELK_RENDER_SHADER_MATERIAL_IMPL_H
#define VELK_RENDER_SHADER_MATERIAL_IMPL_H

#include "spirv_material_reflect.h"

#include <velk/string.h>
#include <velk-render/ext/material.h>
#include <velk-render/interface/intf_draw_data.h>
#include <velk-render/interface/material/intf_material_internal.h>
#include <velk-render/interface/material/intf_shader_material.h>
#include <velk-render/plugin.h>

namespace velk::impl {

/**
 * @brief Material that takes raw GLSL sources from the user and bypasses
 *        the eval-driver composition.
 *
 * Inherits from ext::Material for automatic pipeline-handle invalidation
 * on IMaterialOptions change. Pipeline compilation is the renderer's job
 * (via batch_builder's raw-shader branch) — this class just holds the
 * sources and input properties.
 */
class ShaderMaterial
    : public ext::Material<ShaderMaterial, IShaderMaterial>
{
public:
    VELK_CLASS_UID(ClassId::ShaderMaterial, "ShaderMaterial");

    size_t get_draw_data_size() const override;
    ReturnValue write_draw_data(void* out, size_t size, ITextureResolver* resolver = nullptr) const override;

    // IMaterialInternal
    ReturnValue setup_inputs(const vector<ShaderParam>& params) override;
    void set_sources(string_view vertex_source, string_view fragment_source) override;

    // IShaderSource: ShaderMaterial supplies full vertex + fragment
    // shaders and opts out of the eval-driver composition.
    string_view get_source(string_view role) const override
    {
        if (role == ::velk::shader_role::kVertex)   return vertex_source_;
        if (role == ::velk::shader_role::kFragment) return fragment_source_;
        return {};
    }

private:
    string vertex_source_;
    string fragment_source_;
    vector<ShaderParam> params_;
    size_t gpu_data_size_ = 0;
};

} // namespace velk::impl

#endif // VELK_RENDER_SHADER_MATERIAL_IMPL_H
