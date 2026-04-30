#ifndef VELK_RENDER_FRAME_BATCH_H
#define VELK_RENDER_FRAME_BATCH_H

#include <velk/api/math_types.h>
#include <velk/vector.h>

#include <velk-render/interface/intf_mesh.h>
#include <velk-render/interface/intf_program.h>
#include <velk-render/interface/intf_raster_shader.h>
#include <velk-render/interface/intf_render_backend.h>
#include <velk-render/interface/intf_shader_snippet.h>

#include <cstdint>

namespace velk {

/**
 * @brief One draw-able primitive instance group.
 *
 * Built scene-side by walking the visual list (`BatchBuilder` in
 * velk-scene) and consumed render-side by `build_draw_calls` /
 * `build_gbuffer_draw_calls` (velk-render). The struct itself is
 * scene-agnostic — it carries only renderer-facing references.
 *
 * `pipeline_key` is a stable hash on visual class / material;
 * resolved through `IRenderContext::pipeline_map()`. `texture_key`
 * is the bindless-source ISurface address (resolved at emit time).
 * `instance_data` is per-instance bytes the vertex shader reads via
 * a `buffer_reference`. `world_aabb` is the union of every contained
 * instance's bounds, used by frustum culling at emit time.
 */
struct Batch
{
    uint64_t pipeline_key = 0;
    uint64_t texture_key = 0;
    vector<uint8_t> instance_data;
    uint32_t instance_stride = 0;
    uint32_t instance_count = 0;
    aabb world_aabb = aabb::empty();
    IProgram::Ptr material;
    IShaderSnippet::Ptr visual_discard;
    /// Precomputed key perturbation for the gbuffer pipeline cache;
    /// 0 when the visual contributes no discard snippet.
    uint64_t discard_key_perturb = 0;
    IMeshPrimitive::Ptr primitive;
    IRasterShader::Ptr raster_shader;

    /// Captured at batch-build time so build_draw_calls can lazy-compile
    /// the pipeline against any target format on cache miss without
    /// re-reading the visual / material storage.
    PipelineOptions pipeline_options{};
};

} // namespace velk

#endif // VELK_RENDER_FRAME_BATCH_H
