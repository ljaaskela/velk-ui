#include "deferred_path.h"

#include "default_ui_shaders.h"
#include "render_target_cache.h"

#include <velk/api/perf.h>
#include <velk/api/state.h>
#include <velk/string.h>
#include <velk/interface/intf_object_storage.h>

#include <velk-render/gbuffer.h>
#include <velk-render/gpu_data.h>
#include <velk-render/interface/intf_draw_data.h>
#include <velk-render/interface/intf_program.h>
#include <velk-render/interface/intf_surface.h>
#include <velk-render/interface/intf_window_surface.h>
#include <velk-render/interface/material/intf_material.h>
#include <velk-scene/interface/intf_environment.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace velk {

uint64_t DeferredPath::ensure_pipeline(FrameContext& ctx)
{
    if (!ctx.render_ctx || !ctx.snippets) return 0;

    const auto& intersect_ids = ctx.snippets->frame_intersects();
    const auto& intersect_info_by_id = ctx.snippets->intersect_info_by_id();

    constexpr uint64_t kFnvBasis = 0xcbf29ce484222325ULL;
    constexpr uint64_t kFnvPrime = 0x100000001b3ULL;
    constexpr uint64_t kDeferredTag = 0x44665232'44666572ULL;
    uint64_t key = kFnvBasis ^ kDeferredTag;
    for (auto id : intersect_ids) {
        key = (key ^ static_cast<uint64_t>(id)) * kFnvPrime;
    }
    key |= 0x4000000000000000ULL;

    if (compiled_pipelines_.find(key) != compiled_pipelines_.end()) {
        return key;
    }

    string src;
    src += default_deferred_compute_src;
    for (auto id : intersect_ids) {
        if (id < 3 || id - 3 >= intersect_info_by_id.size()) continue;
        const auto& ii = intersect_info_by_id[id - 3];
        src += string_view("#include \"", 10);
        src += ii.include_name;
        src += string_view("\"\n", 2);
    }

    auto append_literal = [&src](const char* s) {
        src += string_view(s, std::strlen(s));
    };

    append_literal("bool intersect_shape(Ray ray, RtShape shape, out RayHit hit) {\n");
    append_literal("    switch (shape.shape_kind) {\n");
    append_literal("        case 1u: return intersect_cube(ray, shape, hit);\n");
    append_literal("        case 2u: return intersect_sphere(ray, shape, hit);\n");
    append_literal("        case 255u: return intersect_mesh(ray, shape, hit);\n");
    char buf[128];
    for (auto id : intersect_ids) {
        if (id < 3 || id - 3 >= intersect_info_by_id.size()) continue;
        const auto& ii = intersect_info_by_id[id - 3];
        int n = std::snprintf(buf, sizeof(buf), "        case %uu: return ", id);
        if (n > 0) src += string_view(static_cast<const char*>(buf), static_cast<size_t>(n));
        src += ii.fn_name;
        append_literal("(ray, shape, hit);\n");
    }
    append_literal("        default: return intersect_rect(ray, shape, hit);\n");
    append_literal("    }\n");
    append_literal("}\n");

    uint64_t compiled = ctx.render_ctx->compile_compute_pipeline(string_view(src), key);
    if (compiled == 0) return 0;
    compiled_pipelines_[key] = true;
    return key;
}

RenderTargetGroup DeferredPath::ensure_gbuffer(ViewState& vs, int width, int height,
                                               FrameContext& ctx)
{
    if (width <= 0 || height <= 0 || !ctx.backend) return 0;
    if (vs.gbuffer_group != 0 &&
        vs.gbuffer_width == width && vs.gbuffer_height == height) {
        return vs.gbuffer_group;
    }
    if (vs.gbuffer_group != 0) {
        ctx.backend->destroy_render_target_group(vs.gbuffer_group);
        vs.gbuffer_group = 0;
    }
    vs.gbuffer_group = ctx.backend->create_render_target_group(
        array_view<const PixelFormat>(kGBufferFormats,
                                      static_cast<uint32_t>(GBufferAttachment::Count)),
        width, height,
        DepthFormat::Default);
    if (vs.gbuffer_group != 0) {
        vs.gbuffer_width = width;
        vs.gbuffer_height = height;
    }
    return vs.gbuffer_group;
}

void DeferredPath::build_passes(ViewEntry& entry,
                                const SceneState& /*scene_state*/,
                                const RenderView& render_view,
                                FrameContext& ctx,
                                vector<RenderPass>& out_passes)
{
    if (!ctx.backend || !ctx.frame_buffer || !ctx.batch_builder || !ctx.pipeline_map) {
        return;
    }
    if (render_view.width <= 0 || render_view.height <= 0) return;

    auto& vs = view_states_[&entry];

    if (ctx.render_target_cache) {
        ctx.render_target_cache->ensure(ctx);
    }

    auto gbuffer_group = ensure_gbuffer(vs, render_view.width, render_view.height, ctx);
    if (gbuffer_group == 0) return;

    emit_gbuffer_pass(entry, vs, render_view, ctx, out_passes);

    if (vs.gbuffer_width <= 0 || vs.gbuffer_height <= 0) return;
    emit_lighting_pass(entry, vs, render_view, ctx,
                       vs.gbuffer_width, vs.gbuffer_height, out_passes);
}

void DeferredPath::emit_gbuffer_pass(ViewEntry& /*entry*/, ViewState& vs,
                                     const RenderView& render_view, FrameContext& ctx,
                                     vector<RenderPass>& out_passes)
{
    if (!render_view.batches) return;

    auto gbuffer_draw_calls = ctx.render_ctx->build_gbuffer_draw_calls(
        *render_view.batches,
        *ctx.frame_buffer,
        *ctx.resources,
        render_view.frame_globals_addr,
        vs.gbuffer_group,
        ctx.observer,
        ctx.batch_builder->material_addr_cache(),
        render_view.has_frustum ? &render_view.frustum : nullptr);

    RenderPass g_pass;
    g_pass.kind = PassKind::GBufferFill;
    g_pass.gbuffer_group = vs.gbuffer_group;
    g_pass.viewport = {0, 0, static_cast<float>(vs.gbuffer_width),
                       static_cast<float>(vs.gbuffer_height)};
    g_pass.draw_calls = std::move(gbuffer_draw_calls);
    out_passes.push_back(std::move(g_pass));
}

void DeferredPath::emit_lighting_pass(ViewEntry& entry, ViewState& vs,
                                      const RenderView& render_view, FrameContext& ctx,
                                      int w, int h,
                                      vector<RenderPass>& out_passes)
{
    // Allocate / resize the output storage image.
    if (vs.deferred_output_tex != 0 &&
        (vs.deferred_width != w || vs.deferred_height != h)) {
        if (ctx.resources) {
            ctx.resources->defer_texture_destroy(
                vs.deferred_output_tex, ctx.present_counter + ctx.latency_frames);
        }
        vs.deferred_output_tex = 0;
    }
    if (vs.deferred_output_tex == 0) {
        TextureDesc td{};
        td.width = w;
        td.height = h;
        td.format = PixelFormat::RGBA8;
        td.usage = TextureUsage::Storage;
        vs.deferred_output_tex = ctx.backend->create_texture(td);
        vs.deferred_width = w;
        vs.deferred_height = h;
    }
    if (vs.deferred_output_tex == 0) return;

    if (vs.shadow_debug_tex != 0 &&
        (vs.shadow_debug_width != w || vs.shadow_debug_height != h)) {
        if (ctx.resources) {
            ctx.resources->defer_texture_destroy(
                vs.shadow_debug_tex, ctx.present_counter + ctx.latency_frames);
        }
        vs.shadow_debug_tex = 0;
    }
    if (vs.shadow_debug_tex == 0) {
        TextureDesc td{};
        td.width = w;
        td.height = h;
        td.format = PixelFormat::RGBA32F;
        td.usage = TextureUsage::Storage;
        vs.shadow_debug_tex = ctx.backend->create_texture(td);
        vs.shadow_debug_width = w;
        vs.shadow_debug_height = h;
    }

    uint64_t pipeline_key = ensure_pipeline(ctx);
    if (pipeline_key == 0) return;
    auto pit = ctx.pipeline_map->find(pipeline_key);
    if (pit == ctx.pipeline_map->end()) return;

    auto albedo_id   = ctx.backend->get_render_target_group_attachment(
        vs.gbuffer_group, static_cast<uint32_t>(GBufferAttachment::Albedo));
    auto normal_id   = ctx.backend->get_render_target_group_attachment(
        vs.gbuffer_group, static_cast<uint32_t>(GBufferAttachment::Normal));
    auto worldpos_id = ctx.backend->get_render_target_group_attachment(
        vs.gbuffer_group, static_cast<uint32_t>(GBufferAttachment::WorldPos));
    auto material_id = ctx.backend->get_render_target_group_attachment(
        vs.gbuffer_group, static_cast<uint32_t>(GBufferAttachment::MaterialParams));

    // Lights and env come pre-resolved from RenderView. ViewPreparer
    // registered shadow techniques against the snippet registry and
    // stamped flags[1] with the registered id. The deferred compute
    // shader currently hardcodes id=1=rt_shadow; this matches as long
    // as RtShadow is the first registered tech (which it is in samples).
    uint64_t lights_addr = 0;
    if (!render_view.lights.empty() && ctx.frame_buffer) {
        lights_addr = ctx.frame_buffer->write(
            render_view.lights.data(),
            render_view.lights.size() * sizeof(GpuLight));
    }

    VELK_GPU_STRUCT PushC {
        float    cam_pos[4];
        uint32_t output_image_id;
        uint32_t albedo_tex_id;
        uint32_t normal_tex_id;
        uint32_t worldpos_tex_id;
        uint32_t material_tex_id;
        uint32_t width;
        uint32_t height;
        uint32_t light_count;
        uint32_t env_texture_id;
        uint32_t shadow_debug_image_id;
        uint64_t lights_addr;
        uint64_t env_data_addr;
        uint64_t globals_addr;
    };
    static_assert(sizeof(PushC) == 80, "Deferred PushC layout mismatch");

    PushC pc{};
    pc.cam_pos[0] = render_view.cam_pos.x;
    pc.cam_pos[1] = render_view.cam_pos.y;
    pc.cam_pos[2] = render_view.cam_pos.z;
    pc.cam_pos[3] = 0.f;
    pc.output_image_id = vs.deferred_output_tex;
    pc.albedo_tex_id   = albedo_id;
    pc.normal_tex_id   = normal_id;
    pc.worldpos_tex_id = worldpos_id;
    pc.material_tex_id = material_id;
    pc.width  = static_cast<uint32_t>(w);
    pc.height = static_cast<uint32_t>(h);
    pc.light_count = static_cast<uint32_t>(render_view.lights.size());
    pc.env_texture_id = render_view.env.texture_id;
    pc.shadow_debug_image_id = vs.shadow_debug_tex;
    pc.lights_addr = lights_addr;
    pc.env_data_addr = render_view.env.data_addr;
    pc.globals_addr = render_view.frame_globals_addr;

    RenderPass pass;
    pass.kind = PassKind::ComputeBlit;
    pass.compute.pipeline = pit->second;
    pass.compute.groups_x = (w + 7) / 8;
    pass.compute.groups_y = (h + 7) / 8;
    pass.compute.groups_z = 1;
    pass.compute.root_constants_size = sizeof(PushC);
    std::memcpy(pass.compute.root_constants, &pc, sizeof(PushC));
    pass.blit_source = vs.deferred_output_tex;
    pass.blit_surface_id = entry.surface ? entry.surface->get_render_target_id() : 0;
    pass.blit_dst_rect = render_view.viewport;
    out_passes.push_back(std::move(pass));
}

void DeferredPath::on_view_removed(ViewEntry& entry, FrameContext& ctx)
{
    auto it = view_states_.find(&entry);
    if (it == view_states_.end()) return;
    auto& vs = it->second;
    if (vs.gbuffer_group != 0 && ctx.backend) {
        ctx.backend->destroy_render_target_group(vs.gbuffer_group);
    }
    if (vs.deferred_output_tex != 0 && ctx.resources) {
        ctx.resources->defer_texture_destroy(
            vs.deferred_output_tex, ctx.present_counter + ctx.latency_frames);
    }
    if (vs.shadow_debug_tex != 0 && ctx.resources) {
        ctx.resources->defer_texture_destroy(
            vs.shadow_debug_tex, ctx.present_counter + ctx.latency_frames);
    }
    view_states_.erase(it);
}

void DeferredPath::shutdown(FrameContext& ctx)
{
    for (auto& [v, vs] : view_states_) {
        if (vs.gbuffer_group != 0 && ctx.backend) {
            ctx.backend->destroy_render_target_group(vs.gbuffer_group);
        }
        if (vs.deferred_output_tex != 0 && ctx.resources) {
            ctx.resources->defer_texture_destroy(
                vs.deferred_output_tex, ctx.present_counter + ctx.latency_frames);
        }
        if (vs.shadow_debug_tex != 0 && ctx.resources) {
            ctx.resources->defer_texture_destroy(
                vs.shadow_debug_tex, ctx.present_counter + ctx.latency_frames);
        }
    }
    view_states_.clear();
    compiled_pipelines_.clear();
}

RenderTargetGroup DeferredPath::find_gbuffer_group(ViewEntry* view) const
{
    auto it = view_states_.find(view);
    return (it == view_states_.end()) ? 0 : it->second.gbuffer_group;
}

TextureId DeferredPath::find_shadow_debug_tex(ViewEntry* view) const
{
    auto it = view_states_.find(view);
    return (it == view_states_.end()) ? 0 : it->second.shadow_debug_tex;
}

} // namespace velk
