#ifndef VELK_RENDER_SHADER_H
#define VELK_RENDER_SHADER_H

#include <velk/ext/core_object.h>
#include <velk/vector.h>

#include <velk-render/interface/intf_shader.h>

namespace velk {

/// Internal shader implementation. Stores compiled bytecode.
class Shader : public ext::ObjectCore<Shader, IShader>
{
public:
    VELK_CLASS_UID("b3e8f1a2-7c4d-4e9b-a5f6-1d2e3c4b5a68", "Shader");

    void init(vector<uint32_t> bytecode) override
    {
        spirv_ = std::move(bytecode);
    }

    array_view<const uint32_t> get_data() const override { return {spirv_.data(), spirv_.size()}; }

    size_t get_data_size() const override { return spirv_.size() * sizeof(uint32_t); }

private:
    vector<uint32_t> spirv_;
};

} // namespace velk

#endif // VELK_RENDER_SHADER_H
