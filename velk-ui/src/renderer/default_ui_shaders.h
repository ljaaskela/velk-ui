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

// Compute ray tracer prelude. Fixed header that precedes all material
// snippets. Declares extensions, storage-image binding, shape struct,
// push constants, and the rect intersection routine.
[[maybe_unused]] constexpr string_view rt_compute_prelude_src = R"(
#version 450
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#include "velk.glsl"

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(set = 0, binding = 0) uniform sampler2D velk_textures[];
layout(set = 0, binding = 1, rgba8) uniform writeonly image2D gStorageImages[];

struct RtShape {
    vec4 origin;        // xyz = world origin of the rect (local 0,0 point); w reserved
    vec4 u_axis;        // xyz = world direction of the local x axis, scaled by width; w reserved
    vec4 v_axis;        // xyz = world direction of the local y axis, scaled by height; w reserved
    vec4 color;         // rgba base color (used when material_id == 0)
    vec4 params;        // x = corner radius (world units), yzw reserved
    uint material_id;   // 0 = no material, use color; otherwise dispatched via switch
    uint texture_id;    // bindless texture index, 0 when unused
    uint shape_param;   // per-shape material data (e.g. glyph index for text)
    uint _pad;
    uint64_t material_data_addr;
    uint64_t _tail_pad;
};

layout(buffer_reference, std430) readonly buffer ShapeList {
    RtShape data[];
};

layout(push_constant) uniform PC {
    mat4 inv_view_projection;
    vec4 cam_pos;
    uvec4 extras;       // x=image_index, y=width, z=height, w=shape_count
    uvec4 env;          // x=env_material_id (0 = no env), y=env_texture_id
    ShapeList shapes;
    uint64_t env_data_addr;
} pc;

// Ray vs. oriented rect parameterised by (origin + s*u_axis + t*v_axis) for
// s,t in [0,1]. |u_axis|, |v_axis| carry the world-space extents. On hit
// fills out_uv with (s,t), out_t with the ray parameter, and out_normal with
// the rect's world-space normal.
bool intersect_rect(vec3 ro, vec3 rd, vec3 origin, vec3 u_axis, vec3 v_axis,
                    float radius, out vec2 out_uv, out float out_t, out vec3 out_normal)
{
    vec3 normal = cross(u_axis, v_axis);
    float nlen2 = dot(normal, normal);
    if (nlen2 < 1e-12) return false;
    float inv_nlen = inversesqrt(nlen2);
    vec3 n = normal * inv_nlen;

    float denom = dot(rd, n);
    if (abs(denom) < 1e-6) return false;
    float t = dot(origin - ro, n) / denom;
    if (t <= 0.0) return false;

    vec3 p = ro + t * rd;
    vec3 local = p - origin;
    float u_len2 = dot(u_axis, u_axis);
    float v_len2 = dot(v_axis, v_axis);
    float s = dot(local, u_axis) / u_len2;
    float tt = dot(local, v_axis) / v_len2;
    if (s < 0.0 || s > 1.0 || tt < 0.0 || tt > 1.0) return false;

    // Corner radius test in world-space units.
    if (radius > 0.0) {
        float u_len = sqrt(u_len2);
        float v_len = sqrt(v_len2);
        vec2 size_w = vec2(u_len, v_len);
        vec2 p_w = vec2(s * u_len, tt * v_len);
        vec2 half_size = size_w * 0.5;
        vec2 centered = p_w - half_size;
        vec2 d = abs(centered) - half_size + radius;
        float sdf = length(max(d, 0.0)) + min(max(d.x, d.y), 0.0) - radius;
        if (sdf > 0.0) return false;
    }

    out_uv = vec2(s, tt);
    out_t = t;
    out_normal = n;
    return true;
}
)";

// Compute ray tracer main body. Follows the material snippets and the
// composer-generated velk_resolve_fill dispatch. All shapes are on the
// z=0 plane today; painter's algorithm in enumeration order handles
// overlap via src-over alpha compositing.
[[maybe_unused]] constexpr string_view rt_compute_main_src = R"(
void main()
{
    ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
    uint w = pc.extras.y;
    uint h = pc.extras.z;
    uint shape_count = pc.extras.w;
    if (coord.x >= int(w) || coord.y >= int(h)) return;

    vec2 ndc = (vec2(coord) + 0.5) / vec2(float(w), float(h)) * 2.0 - 1.0;

    vec4 near_h = pc.inv_view_projection * vec4(ndc, 0.0, 1.0);
    vec4 far_h  = pc.inv_view_projection * vec4(ndc, 1.0, 1.0);
    vec3 near_w = near_h.xyz / near_h.w;
    vec3 far_w  = far_h.xyz  / far_h.w;

    vec3 ray_origin = near_w;
    vec3 ray_dir    = normalize(far_w - near_w);

    // Environment as background, if the camera has one. Otherwise opaque black.
    vec4 accum = vec4(0.0, 0.0, 0.0, 1.0);
    if (pc.env.x != 0u) {
        accum = velk_resolve_fill(pc.env.x, pc.env_data_addr, pc.env.y,
                                  0u, vec2(0.0), vec4(1.0), ray_dir);
    }

    for (uint i = 0u; i < shape_count; ++i) {
        RtShape s = pc.shapes.data[i];
        vec2 hit_uv;
        float t;
        vec3 hit_normal;
        if (intersect_rect(ray_origin, ray_dir,
                           s.origin.xyz, s.u_axis.xyz, s.v_axis.xyz,
                           s.params.x, hit_uv, t, hit_normal)) {
            vec4 c = velk_resolve_fill(s.material_id, s.material_data_addr,
                                       s.texture_id, s.shape_param, hit_uv, s.color,
                                       ray_dir);
            accum.rgb = c.rgb * c.a + accum.rgb * (1.0 - c.a);
        }
    }

    imageStore(gStorageImages[nonuniformEXT(pc.extras.x)], coord, accum);
}
)";

} // namespace velk

#endif // VELK_RENDER_DEFAULT_SHADERS_H
