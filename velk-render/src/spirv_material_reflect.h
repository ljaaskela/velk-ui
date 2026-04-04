#ifndef VELK_RENDER_SPIRV_MATERIAL_REFLECT_H
#define VELK_RENDER_SPIRV_MATERIAL_REFLECT_H

#include <velk/string.h>
#include <velk/uid.h>
#include <velk/vector.h>

#include <cstdint>

namespace velk {

/// Describes a single material parameter discovered in a shader's DrawData struct.
struct ShaderParam
{
    string name;
    Uid type_uid;    ///< velk type UID (e.g. type_uid<float>(), type_uid<color>())
    uint32_t offset; ///< Byte offset within the material data (relative to DrawDataHeader end)
    uint32_t size;   ///< Size in bytes
};

/**
 * @brief Reflects material parameters from compiled SPIR-V bytecode.
 *
 * Looks for a buffer_reference type named "DrawData" and extracts fields
 * that follow the standard header (globals, instances, texture_id,
 * instance_count, _pad0, _pad1 = 32 bytes).
 *
 * Fields starting with '_' are treated as padding and skipped.
 *
 * @param spirv  SPIR-V bytecode (from vertex or fragment shader).
 * @param word_count  Number of 32-bit words in the bytecode.
 * @return Material parameters. Empty if no material fields found.
 */
vector<ShaderParam> reflect_material_params(const uint32_t* spirv, size_t word_count);

} // namespace velk

#endif // VELK_RENDER_SPIRV_MATERIAL_REFLECT_H
