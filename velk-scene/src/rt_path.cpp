#include "rt_path.h"

#include "default_ui_shaders.h"
#include "env_helper.h"
#include "frame_snippet_registry.h"
#include "scene_collector.h"

#include <velk/api/perf.h>
#include <velk/api/state.h>
#include <velk/interface/intf_object_storage.h>
#include <velk/string.h>

#include <velk-render/interface/intf_draw_data.h>
#include <velk-render/interface/material/intf_material.h>
#include <velk-render/interface/intf_program.h>
#include <velk-render/interface/intf_render_technique.h>
#include <velk-render/interface/intf_shadow_technique.h>
#include <velk-render/interface/intf_surface.h>
#include <velk-render/interface/intf_camera.h>
#include <velk-scene/interface/intf_environment.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace velk {

namespace {

// Same ortho builder the raster path uses as a fallback when no camera
// is attached. Duplicated here to keep the RT unit self-contained.
void build_ortho_projection(float* out, float width, float height)
{
    std::memset(out, 0, 16 * sizeof(float));
    out[0] = 2.0f / width;
    out[5] = 2.0f / height;
    out[10] = -1.0f;
    out[12] = -1.0f;
    out[13] = -1.0f;
    out[15] = 1.0f;
}

} // namespace

uint64_t RtPath::ensure_pipeline(FrameContext& ctx)
{
    if (!ctx.render_ctx || !ctx.snippets) {
        return 0;
    }

    const auto& material_ids = ctx.snippets->frame_materials();
    const auto& shadow_tech_ids = ctx.snippets->frame_shadow_techs();
    const auto& intersect_ids = ctx.snippets->frame_intersects();

    // Pipeline cache key: FNV-1a across the sorted material / shadow /
    // intersect id lists. Different snippet combos compose to
    // different shaders, so they need distinct cache entries.
    constexpr uint64_t kFnvBasis = 0xcbf29ce484222325ULL;
    constexpr uint64_t kFnvPrime = 0x100000001b3ULL;
    constexpr uint64_t kRtTag = 0x5274436f6d702000ULL;
    uint64_t key = kFnvBasis ^ kRtTag;
    for (auto id : material_ids) {
        key = (key ^ static_cast<uint64_t>(id)) * kFnvPrime;
    }
    key = (key ^ 0xdeadbeefULL) * kFnvPrime;
    for (auto id : shadow_tech_ids) {
        key = (key ^ static_cast<uint64_t>(id)) * kFnvPrime;
    }
    key = (key ^ 0xfeedfaceULL) * kFnvPrime;
    for (auto id : intersect_ids) {
        key = (key ^ static_cast<uint64_t>(id)) * kFnvPrime;
    }
    key |= 0x8000000000000000ULL;

    auto cached = compiled_pipelines_.find(key);
    if (cached != compiled_pipelines_.end()) {
        return key;
    }

    string src = compose_rt_compute(*ctx.snippets);

    uint64_t compiled = ctx.render_ctx->compile_compute_pipeline(string_view(src), key);
    if (compiled == 0) {
        return 0;
    }
    compiled_pipelines_[key] = true;
    return key;
}

void RtPath::build_passes(ViewEntry& entry,
                             const SceneState& /*scene_state*/,
                             const RenderView& render_view,
                             FrameContext& ctx,
                             vector<RenderPass>& out_passes)
{
    if (!ctx.backend || !ctx.render_ctx || !ctx.frame_buffer || !ctx.resources ||
        !ctx.pipeline_map) {
        return;
    }
    if (render_view.width <= 0 || render_view.height <= 0) {
        return;
    }
    int vp_w = render_view.width;
    int vp_h = render_view.height;

    auto& vs = view_states_[&entry];

    // (Re)create storage output texture sized to the viewport.
    if (vs.rt_output_tex != 0 &&
        (vs.width != vp_w || vs.height != vp_h)) {
        ctx.resources->defer_texture_destroy(
            vs.rt_output_tex, ctx.present_counter + ctx.latency_frames);
        vs.rt_output_tex = 0;
    }
    if (vs.rt_output_tex == 0) {
        TextureDesc td{};
        td.width = vp_w;
        td.height = vp_h;
        td.format = PixelFormat::RGBA8;
        td.usage = TextureUsage::Storage;
        vs.rt_output_tex = ctx.backend->create_texture(td);
        vs.width = vp_w;
        vs.height = vp_h;
    }
    if (vs.rt_output_tex == 0) {
        return;
    }

    uint64_t lights_addr = 0;
    if (!render_view.lights.empty()) {
        lights_addr = ctx.frame_buffer->write(
            render_view.lights.data(),
            render_view.lights.size() * sizeof(GpuLight));
    }

    // Make a working copy of the shapes so we can plane-sort without
    // affecting other consumers of render_view.shapes (today nobody
    // else uses it, but RenderView is immutable from a path's POV).
    vector<RtShape> shapes(render_view.shapes);

    uint64_t rt_pipeline_key = ensure_pipeline(ctx);
    if (rt_pipeline_key == 0) {
        return;
    }
    auto pit = ctx.pipeline_map->find(rt_pipeline_key);
    if (pit == ctx.pipeline_map->end()) {
        return;
    }

    // Plane-grouped back-to-front painter sort for the primary buffer.
    // Shapes that share a plane stay in enumeration order via
    // stable_sort, preserving authored layering on a flat UI panel.
    // Shapes on different planes sort by NDC depth of a representative
    // origin so stacked 3D panels composite back-to-front.
    const mat4& vp_mat = render_view.view_projection;
    if (shapes.size() > 1) {
        auto plane_key = [](const RtShape& s) -> uint64_t {
            float ux = s.u_axis[0], uy = s.u_axis[1], uz = s.u_axis[2];
            float vx = s.v_axis[0], vy = s.v_axis[1], vz = s.v_axis[2];
            float nx_r = uy * vz - uz * vy;
            float ny_r = uz * vx - ux * vz;
            float nz_r = ux * vy - uy * vx;
            float nlen = std::sqrt(nx_r * nx_r + ny_r * ny_r + nz_r * nz_r);
            if (nlen < 1e-6f) nlen = 1.f;
            float nx = nx_r / nlen;
            float ny = ny_r / nlen;
            float nz = nz_r / nlen;
            float offset = s.origin[0] * nx + s.origin[1] * ny + s.origin[2] * nz;
            int32_t qnx = static_cast<int32_t>(std::round(nx * 1000.f));
            int32_t qny = static_cast<int32_t>(std::round(ny * 1000.f));
            int32_t qnz = static_cast<int32_t>(std::round(nz * 1000.f));
            int32_t qo  = static_cast<int32_t>(std::round(offset * 100.f));
            uint64_t h = 0xcbf29ce484222325ULL;
            auto mix = [&h](uint32_t v) { h = (h ^ v) * 0x100000001b3ULL; };
            mix(static_cast<uint32_t>(qnx));
            mix(static_cast<uint32_t>(qny));
            mix(static_cast<uint32_t>(qnz));
            mix(static_cast<uint32_t>(qo));
            return h;
        };

        vector<uint64_t> keys(shapes.size());
        std::unordered_map<uint64_t, float> plane_depth;
        for (size_t i = 0; i < shapes.size(); ++i) {
            keys[i] = plane_key(shapes[i]);
            if (plane_depth.count(keys[i])) continue;
            const auto& s = shapes[i];
            float x = s.origin[0], y = s.origin[1], z = s.origin[2];
            float cz = vp_mat(2, 0) * x + vp_mat(2, 1) * y + vp_mat(2, 2) * z + vp_mat(2, 3);
            float cw = vp_mat(3, 0) * x + vp_mat(3, 1) * y + vp_mat(3, 2) * z + vp_mat(3, 3);
            plane_depth[keys[i]] = (cw != 0.f) ? (cz / cw) : 0.f;
        }

        vector<size_t> order(shapes.size());
        for (size_t i = 0; i < order.size(); ++i) order[i] = i;
        std::stable_sort(order.begin(), order.end(),
            [&](size_t a, size_t b) {
                uint64_t ka = keys[a];
                uint64_t kb = keys[b];
                if (ka == kb) return false;
                return plane_depth[ka] > plane_depth[kb];
            });

        vector<RtShape> sorted_shapes(shapes.size());
        for (size_t i = 0; i < order.size(); ++i) sorted_shapes[i] = shapes[order[i]];
        shapes = std::move(sorted_shapes);
    }

    uint64_t shapes_addr = 0;
    if (!shapes.empty()) {
        shapes_addr = ctx.frame_buffer->write(
            shapes.data(), shapes.size() * sizeof(RtShape));
    }

    VELK_GPU_STRUCT PushC {
        float inv_vp[16];
        float cam_pos[4];
        uint32_t image_index;
        uint32_t width;
        uint32_t height;
        uint32_t shape_count;
        uint32_t env_material_id;
        uint32_t env_texture_id;
        uint32_t frame_counter;
        uint32_t _env_pad1;
        uint64_t shapes_addr;
        uint64_t bvh_shapes_addr;
        uint64_t bvh_nodes_addr;
        uint32_t bvh_root;
        uint32_t bvh_node_count;
        uint64_t env_data_addr;
        uint64_t lights_addr;
        uint32_t light_count;
        uint32_t _lights_pad;
        uint64_t globals_addr;
    };

    PushC pc{};
    std::memcpy(pc.inv_vp, render_view.inverse_view_projection.m, sizeof(pc.inv_vp));
    pc.cam_pos[0] = render_view.cam_pos.x;
    pc.cam_pos[1] = render_view.cam_pos.y;
    pc.cam_pos[2] = render_view.cam_pos.z;
    pc.cam_pos[3] = 0.f;
    pc.image_index = vs.rt_output_tex;
    pc.width = static_cast<uint32_t>(vp_w);
    pc.height = static_cast<uint32_t>(vp_h);
    pc.shape_count = static_cast<uint32_t>(shapes.size());
    pc.env_material_id = render_view.env.material_id;
    pc.env_texture_id = render_view.env.texture_id;
    pc.frame_counter = static_cast<uint32_t>(ctx.present_counter);
    pc.shapes_addr = shapes_addr;
    pc.bvh_shapes_addr = render_view.bvh_shapes_addr;
    pc.bvh_nodes_addr = render_view.bvh_nodes_addr;
    pc.bvh_root = render_view.bvh_root;
    pc.bvh_node_count = render_view.bvh_node_count;
    pc.env_data_addr = render_view.env.data_addr;
    pc.lights_addr = lights_addr;
    pc.light_count = static_cast<uint32_t>(render_view.lights.size());
    pc.globals_addr = render_view.frame_globals_addr;

    RenderPass pass;
    pass.kind = PassKind::ComputeBlit;
    pass.compute.pipeline = pit->second;
    pass.compute.groups_x = (vp_w + 7) / 8;
    pass.compute.groups_y = (vp_h + 7) / 8;
    pass.compute.groups_z = 1;
    pass.compute.root_constants_size = sizeof(PushC);
    std::memcpy(pass.compute.root_constants, &pc, sizeof(PushC));
    pass.blit_source = vs.rt_output_tex;
    pass.blit_surface_id = entry.surface ? entry.surface->get_render_target_id() : 0;
    pass.blit_dst_rect = render_view.viewport;
    out_passes.push_back(std::move(pass));
}

void RtPath::on_view_removed(ViewEntry& entry, FrameContext& ctx)
{
    auto it = view_states_.find(&entry);
    if (it == view_states_.end()) return;
    if (it->second.rt_output_tex != 0 && ctx.resources) {
        ctx.resources->defer_texture_destroy(
            it->second.rt_output_tex, ctx.present_counter + ctx.latency_frames);
    }
    view_states_.erase(it);
}

void RtPath::shutdown(FrameContext& ctx)
{
    if (ctx.resources) {
        for (auto& [v, vs] : view_states_) {
            if (vs.rt_output_tex != 0) {
                ctx.resources->defer_texture_destroy(
                    vs.rt_output_tex, ctx.present_counter + ctx.latency_frames);
            }
        }
    }
    view_states_.clear();
}

} // namespace velk
