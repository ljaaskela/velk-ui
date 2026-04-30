#include "forward_path.h"

#include <velk/string.h>

#include <velk-render/frame/draw_call_emit.h>
#include <velk-render/frame/raster_shaders.h>
#include <velk-render/interface/intf_raster_shader.h>
#include <velk-render/interface/intf_render_context.h>
#include <velk-render/interface/intf_render_target.h>
#include <velk-render/interface/intf_surface.h>
#include <velk-render/interface/material/intf_material.h>

namespace velk {

namespace {

/// Resolves (or lazy-compiles) the forward-rendering pipeline for a
/// batch. Material wins over the visual's IRasterShader when both are
/// present; the visual's raster_shader is the no-material fallback.
/// Returns 0 to skip (no source / compile failure).
PipelineId resolve_or_compile_forward(IRenderContext& ctx,
                                      const Batch& batch,
                                      PixelFormat target_format)
{
    const bool use_material = (batch.material != nullptr);
    uint64_t user_key = use_material
        ? batch.material->get_pipeline_handle(ctx)
        : batch.pipeline_key;

    auto& pipeline_map = ctx.pipeline_map();
    if (auto it = pipeline_map.find(
            PipelineCacheKey{user_key, target_format, 0});
        it != pipeline_map.end()) {
        return it->second;
    }

    uint64_t compiled_key = 0;
    if (use_material) {
        if (auto* mat = interface_cast<IMaterial>(batch.material.get())) {
            auto eval_src = mat->get_eval_src();
            auto vertex_src = mat->get_vertex_src();
            auto eval_fn = mat->get_eval_fn_name();
            auto frag_src = mat->get_fragment_src();
            if (!eval_src.empty() && !vertex_src.empty() && !eval_fn.empty()) {
                mat->register_eval_includes(ctx);
                string frag = compose_eval_fragment(
                    forward_fragment_driver_template, eval_src, eval_fn,
                    mat->get_forward_discard_threshold());
                compiled_key = ctx.compile_pipeline(
                    string_view(frag), vertex_src,
                    user_key, target_format, 0,
                    batch.pipeline_options);
            } else if (!frag_src.empty() && !vertex_src.empty()) {
                compiled_key = ctx.compile_pipeline(
                    frag_src, vertex_src,
                    user_key, target_format, 0,
                    batch.pipeline_options);
            }
            if (compiled_key && batch.material->get_pipeline_handle(ctx) == 0) {
                batch.material->set_pipeline_handle(compiled_key);
            }
        }
    } else if (batch.raster_shader && user_key != 0) {
        auto src = batch.raster_shader->get_raster_source(
            IRasterShader::Target::Forward);
        compiled_key = ctx.compile_pipeline(
            src.fragment, src.vertex,
            user_key, target_format, 0,
            batch.pipeline_options);
    }

    if (compiled_key == 0) return 0;

    if (auto it = pipeline_map.find(
            PipelineCacheKey{compiled_key, target_format, 0});
        it != pipeline_map.end()) {
        return it->second;
    }
    return 0;
}

} // namespace

void ForwardPath::build_passes(ViewEntry& entry,
                               const RenderView& render_view,
                               IRenderTarget::Ptr color_target,
                               FrameContext& ctx,
                               IRenderGraph& graph)
{
    if (!ctx.backend || !ctx.frame_buffer || !ctx.material_cache
        || !ctx.pipeline_map || !ctx.render_ctx) {
        return;
    }
    if (render_view.width <= 0 || render_view.height <= 0) return;

    const ::velk::render::Frustum* frustum_ptr =
        render_view.has_frustum ? &render_view.frustum : nullptr;
    auto& mcache = *ctx.material_cache;

    auto* default_uv1 = ctx.render_ctx->get_default_buffer(DefaultBufferType::Uv1).get();
    auto target_format = ctx.target_format;

    auto resolve = [&](const Batch& b) {
        return resolve_or_compile_forward(*ctx.render_ctx, b, target_format);
    };

    vector<DrawCall> draw_calls;

    // Env first (no frustum cull — fullscreen). Empty material =>
    // no env on this view's camera.
    if (render_view.env_batch.material) {
        vector<Batch> env_batches;
        env_batches.push_back(render_view.env_batch);
        auto env_draws = emit_draw_calls(
            env_batches, *ctx.frame_buffer, *ctx.resources,
            render_view.frame_globals_addr, ctx.observer, mcache,
            default_uv1, resolve, /*frustum=*/nullptr);
        for (auto& dc : env_draws) {
            draw_calls.push_back(std::move(dc));
        }
    }

    // Main scene batches.
    if (render_view.batches && !render_view.batches->empty()) {
        auto main_draws = emit_draw_calls(
            *render_view.batches, *ctx.frame_buffer, *ctx.resources,
            render_view.frame_globals_addr, ctx.observer, mcache,
            default_uv1, resolve, frustum_ptr);
        for (auto& dc : main_draws) {
            draw_calls.push_back(std::move(dc));
        }
    }

    RenderPass pass;
    pass.target.target = color_target;
    pass.viewport = render_view.viewport;
    pass.draw_calls = std::move(draw_calls);
    graph.add_pass(std::move(pass));
}

} // namespace velk
