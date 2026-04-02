#include "gradient_material.h"

namespace velk_ui {

namespace {

const char* gradient_vertex_src = R"(
#version 330 core

const vec2 quad[4] = vec2[4](
    vec2(0.0, 0.0),
    vec2(1.0, 0.0),
    vec2(0.0, 1.0),
    vec2(1.0, 1.0)
);

layout(location = 0) in vec4 inst_rect;
layout(location = 1) in vec4 inst_color;

uniform mat4 u_projection;

out vec2 v_local_uv;

void main()
{
    vec2 pos = quad[gl_VertexID];
    vec2 world_pos = inst_rect.xy + pos * inst_rect.zw;
    gl_Position = u_projection * vec4(world_pos, 0.0, 1.0);
    v_local_uv = pos;
}
)";

const char* gradient_fragment_src = R"(
#version 330 core

in vec2 v_local_uv;
out vec4 frag_color;

uniform vec4 start_color;
uniform vec4 end_color;
uniform float angle;

void main()
{
    float rad = radians(angle);
    vec2 dir = vec2(cos(rad), sin(rad));
    float t = dot(v_local_uv - 0.5, dir) + 0.5;
    t = clamp(t, 0.0, 1.0);
    frag_color = mix(start_color, end_color, t);
}
)";

} // namespace

uint64_t GradientMaterial::get_pipeline_handle(IRenderContext& ctx)
{
    if (!shader_mat_) {
        auto obj = ctx.create_shader_material(gradient_fragment_src, gradient_vertex_src);
        if (obj) {
            shader_mat_ = interface_pointer_cast<IMaterial>(obj);
        }
    }
    return shader_mat_ ? shader_mat_->get_pipeline_handle(ctx) : 0;
}

} // namespace velk_ui
