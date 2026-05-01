#ifndef VELK_RENDER_SHADER_COMPILER_H
#define VELK_RENDER_SHADER_COMPILER_H

#include <velk/string.h>
#include <velk/vector.h>

#include <velk-render/interface/intf_shader.h>

#include <cstdint>
#include <unordered_map>

namespace velk {

/// Registry of virtual shader include files (name -> content).
using ShaderIncludeMap = std::unordered_map<string, string>;

/// Source for the framework-level "velk.glsl" virtual include. Registered
/// automatically into every RenderContext during init.
extern const char* kVelkGlsl;

/// Compiles GLSL to SPIR-V. Resolves #include directives against the supplied
/// include map (which should already contain "velk.glsl" and any plugin
/// includes such as "velk-ui.glsl").
vector<uint32_t> compile_glsl_to_spirv(string_view source, ShaderStage stage,
                                       const ShaderIncludeMap* user_includes = nullptr);

} // namespace velk

#endif // VELK_RENDER_SHADER_COMPILER_H
