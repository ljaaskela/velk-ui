#include "view_preparer.h"

#include "batch_builder.h"
#include "env_helper.h"
#include "scene_collector.h"

#include <velk/api/perf.h>
#include <velk/api/state.h>
#include <velk/interface/intf_object_storage.h>

#include <velk-render/gpu_data.h>
#include <velk-render/interface/intf_camera.h>
#include <velk-render/interface/intf_draw_data.h>
#include <velk-render/interface/intf_program.h>
#include <velk-render/interface/intf_shadow_technique.h>
#include <velk-render/interface/intf_window_surface.h>
#include <velk-render/interface/material/intf_material.h>
#include <velk-scene/interface/intf_environment.h>

#include <cstdlib>
#include <cstring>

namespace velk {

namespace {

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

RenderView ViewPreparer::prepare(ViewEntry& entry,
                                 const SceneState& scene_state,
                                 FrameContext& ctx)
{
    VELK_PERF_SCOPE("renderer.view_prepare");
    RenderView rv;

    auto camera = ::velk::find_attachment<ICamera>(entry.camera_element);

    // Raster batch cache: rebuild on dirty, reuse otherwise.
    auto& cache = view_caches_[&entry];
    if (entry.batches_dirty && ctx.batch_builder) {
        ctx.batch_builder->rebuild_batches(scene_state, cache.batches);
        entry.batches_dirty = false;
    }
    rv.batches = &cache.batches;

    // Viewport in pixels.
    auto sstate = read_state<IWindowSurface>(entry.surface);
    float sw = static_cast<float>(sstate ? sstate->size.x : 0);
    float sh = static_cast<float>(sstate ? sstate->size.y : 0);
    bool has_viewport = entry.viewport.width > 0 && entry.viewport.height > 0;
    float vp_x = has_viewport ? entry.viewport.x * sw : 0.f;
    float vp_y = has_viewport ? entry.viewport.y * sh : 0.f;
    float vp_w = has_viewport ? entry.viewport.width * sw : sw;
    float vp_h = has_viewport ? entry.viewport.height * sh : sh;
    rv.viewport = {vp_x, vp_y, vp_w, vp_h};
    rv.width = static_cast<int>(vp_w);
    rv.height = static_cast<int>(vp_h);

    // Camera matrices. No camera → ortho fallback covering the viewport.
    if (vp_w > 0 && vp_h > 0) {
        if (camera) {
            auto cam_es = read_state<IElement>(entry.camera_element);
            mat4 cam_world = cam_es ? cam_es->world_matrix : mat4::identity();
            rv.view_projection = camera->get_view_projection(cam_world, vp_w, vp_h);
            if (cam_es) {
                rv.cam_pos.x = cam_es->world_matrix(0, 3);
                rv.cam_pos.y = cam_es->world_matrix(1, 3);
                rv.cam_pos.z = cam_es->world_matrix(2, 3);
            }
            rv.frustum = ::velk::render::extract_frustum(rv.view_projection);
            rv.has_frustum = true;
        } else {
            float ortho[16];
            build_ortho_projection(ortho, vp_w, vp_h);
            std::memcpy(rv.view_projection.m, ortho, sizeof(ortho));
        }
        rv.inverse_view_projection = mat4::inverse(rv.view_projection);
    }

    // BVH addresses (populated scene-wide on FrameContext earlier in the
    // frame; copy into RenderView so paths don't need both).
    rv.bvh_nodes_addr = ctx.bvh_nodes_addr;
    rv.bvh_shapes_addr = ctx.bvh_shapes_addr;
    rv.bvh_root = ctx.bvh_root;
    rv.bvh_node_count = ctx.bvh_node_count;
    rv.bvh_shape_count = ctx.bvh_shape_count;

    // FrameGlobals: shared GPU block read by every path's pipeline via
    // pointer. Mirrors what each path used to compute on its own.
    if (vp_w > 0 && vp_h > 0 && ctx.frame_buffer) {
        FrameGlobals globals{};
        std::memcpy(globals.view_projection, rv.view_projection.m, sizeof(rv.view_projection.m));
        std::memcpy(globals.inverse_view_projection, rv.inverse_view_projection.m,
                    sizeof(rv.inverse_view_projection.m));
        globals.viewport[0] = vp_w;
        globals.viewport[1] = vp_h;
        globals.viewport[2] = 1.0f / vp_w;
        globals.viewport[3] = 1.0f / vp_h;
        globals.cam_pos[0] = rv.cam_pos.x;
        globals.cam_pos[1] = rv.cam_pos.y;
        globals.cam_pos[2] = rv.cam_pos.z;
        globals.bvh_root = rv.bvh_root;
        globals.bvh_node_count = rv.bvh_node_count;
        globals.bvh_shape_count = rv.bvh_shape_count;
        globals.bvh_nodes_addr = rv.bvh_nodes_addr;
        globals.bvh_shapes_addr = rv.bvh_shapes_addr;
        rv.frame_globals_addr = ctx.frame_buffer->write(&globals, sizeof(globals));
    }

    // Scene lights: register shadow tech (if any) with the snippet
    // registry so RT and deferred dispatch see the same id assignment,
    // then push the (potentially modified) base record into RenderView.
    struct LightCollect {
        FrameContext& ctx;
        vector<GpuLight>& out;
    };
    LightCollect lc{ctx, rv.lights};
    enumerate_scene_lights(scene_state,
        +[](void* u, LightSite& site) {
            auto& s = *static_cast<LightCollect*>(u);
            if (auto tech = find_shadow_technique(site.light)) {
                site.base.flags[1] = s.ctx.snippets
                    ? s.ctx.snippets->register_shadow_tech(tech.get(), *s.ctx.render_ctx)
                    : 0;
            }
            s.out.push_back(site.base);
        }, &lc);

    // Camera environment.
    if (camera) {
        auto resolved = ensure_env_ready(*camera, ctx);
        if (resolved.env) {
            rv.env.texture_id = resolved.texture_id;
            if (auto env_mat = resolved.env->get_material()) {
                auto env_prog = interface_pointer_cast<IProgram>(env_mat);
                if (ctx.snippets) {
                    auto env_ref =
                        ctx.snippets->resolve_material(env_prog.get(), ctx.make_resolve_context());
                    rv.env.material_id = env_ref.mat_id;
                    rv.env.data_addr = env_ref.mat_addr;
                }
                // DeferredPath uses a hand-serialized env data block
                // instead of the snippet-registry one. Provide that path
                // by serializing into the frame buffer here too. If the
                // snippet registry already produced an addr, prefer it
                // (more efficient — single upload per program per frame).
                if (rv.env.data_addr == 0 && ctx.frame_buffer) {
                    if (auto* dd = interface_cast<IDrawData>(env_prog.get())) {
                        size_t sz = dd->get_draw_data_size();
                        if (sz > 0) {
                            void* scratch = std::malloc(sz);
                            if (scratch) {
                                std::memset(scratch, 0, sz);
                                if (dd->write_draw_data(scratch, sz) == ReturnValue::Success) {
                                    rv.env.data_addr = ctx.frame_buffer->write(scratch, sz);
                                }
                                std::free(scratch);
                            }
                        }
                    }
                }
            }
        }
    }

    return rv;
}

void ViewPreparer::on_view_removed(ViewEntry& entry)
{
    view_caches_.erase(&entry);
}

void ViewPreparer::clear()
{
    view_caches_.clear();
}

} // namespace velk
