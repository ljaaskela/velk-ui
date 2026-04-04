#ifndef VELK_RENDER_GPU_DATA_H
#define VELK_RENDER_GPU_DATA_H

#include <cstdint>

namespace velk {

// Framework-level GPU data structures.
//
// Visual-specific instance types (RectInstance, TextInstance, etc.) are defined
// by the visuals that produce them, not here.
//
// Use VELK_GPU_STRUCT for material data structs. It ensures 16-byte alignment
// (matching std430 vec4 alignment) so the compiler pads the struct automatically.

/// Declares a struct with std430-compatible alignment (16 bytes).
/// Use for material GPU data structs that follow the DrawDataHeader.
#define VELK_GPU_STRUCT struct alignas(16)

struct FrameGlobals
{
    float projection[16];
    float viewport[4]; // width, height, 1/width, 1/height
};

// Standard draw data header written by the renderer into the staging buffer.
// The shader receives a pointer to this via push constants.
// Material-specific data follows immediately after.
VELK_GPU_STRUCT DrawDataHeader
{
    uint64_t globals_address;
    uint64_t instances_address;
    uint32_t texture_id;
    uint32_t instance_count;
};

static_assert(sizeof(DrawDataHeader) == 32, "DrawDataHeader must be 32 bytes for std430 alignment");

} // namespace velk

#endif // VELK_RENDER_GPU_DATA_H
