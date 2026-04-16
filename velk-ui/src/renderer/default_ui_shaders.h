#ifndef VELK_RENDER_DEFAULT_SHADERS_H
#define VELK_RENDER_DEFAULT_SHADERS_H

#include <velk/string_view.h>

namespace velk {

// The default vertex/fragment shader pair used when a visual or material
// does not supply its own. The vertex shader expands the unit quad to a
// world-space rect and emits the varyings every known consumer reads:
//   location 0: v_color    (vec4) - instance color
//   location 1: v_local_uv (vec2) - unit quad coords, 0..1 across the rect
//   location 2: v_size     (vec2, flat) - the rect's world-space size
// Fragments that do not read v_local_uv / v_size simply ignore them.
//
// Built-in shaders use:
//   #include "velk.glsl"    - framework types (GlobalData, Ptr64, velk_unit_quad, VELK_DRAW_DATA)
//   #include "velk-ui.glsl" - UI instance types (RectInstance, TextInstance)

[[maybe_unused]] constexpr string_view default_vertex_src = R"(
#version 450
#include "velk.glsl"
#include "velk-ui.glsl"

layout(buffer_reference, std430) readonly buffer DrawData {
    VELK_DRAW_DATA(RectInstanceData)
};

layout(push_constant) uniform PC { DrawData root; };

layout(location = 0) out vec4 v_color;
layout(location = 1) out vec2 v_local_uv;
layout(location = 2) flat out vec2 v_size;

void main()
{
    vec2 q = velk_unit_quad(gl_VertexIndex);
    RectInstance inst = root.instance_data.data[gl_InstanceIndex];
    vec2 world_pos = inst.pos + q * inst.size;
    gl_Position = root.global_data.view_projection * vec4(world_pos, 0.0, 1.0);
    v_color = inst.color;
    v_local_uv = q;
    v_size = inst.size;
}
)";

[[maybe_unused]] constexpr string_view default_fragment_src = R"(
#version 450

layout(location = 0) in vec4 v_color;
layout(location = 0) out vec4 frag_color;

void main()
{
    frag_color = v_color;
}
)";

} // namespace velk

#endif // VELK_RENDER_DEFAULT_SHADERS_H
