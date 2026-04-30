#include "render_target_cache.h"

#include "batch_builder.h"

#include <velk/api/state.h>
#include <velk/interface/intf_object_storage.h>

#include <velk-render/frame/render_view.h>
#include <velk-render/gpu_data.h>
#include <velk-render/interface/intf_render_backend.h>
#include <velk-scene/interface/intf_render_to_texture.h>

#include <velk/api/velk.h>

#include <algorithm>
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

void RenderTargetCache::ensure(FrameContext& ctx, BatchBuilder& batch_builder)
{
    if (!ctx.backend || !ctx.resources) {
        return;
    }
    for (auto& rtp : batch_builder.render_target_passes()) {
        auto& rte = entries_[rtp.element];
        if (!rte.target) {
            if (auto rtt = ::velk::find_attachment<IRenderToTexture>(rtp.element)) {
                if (auto rtt_state = read_state<IRenderToTexture>(rtt)) {
                    rte.target = rtt_state->render_target.get<IRenderTarget>();
                }
            }
        }
        if (!rte.target) continue;

        int w{1}, h{1};
        if (auto es = read_state<IElement>(rtp.element)) {
            w = std::max(static_cast<int>(es->size.width), 1);
            h = std::max(static_cast<int>(es->size.height), 1);
        }
        if (rte.texture_id != 0 && (rte.width != w || rte.height != h)) {
            ctx.resources->defer_texture_destroy(
                rte.texture_id, ctx.present_counter + ctx.latency_frames);
            rte.texture_id = 0;
        }
        if (rte.texture_id == 0) {
            TextureDesc tdesc{};
            tdesc.width = w;
            tdesc.height = h;
            // Element-cache RTT must match the surface format so the
            // swapchain-compiled UI pipelines remain render-pass compatible.
            tdesc.format = PixelFormat::Surface;
            tdesc.usage = TextureUsage::RenderTarget;
            rte.texture_id = ctx.backend->create_texture(tdesc);
            rte.width = w;
            rte.height = h;
            if (rte.texture_id != 0) {
                rte.target->set_gpu_handle(GpuResourceKey::Default, static_cast<uint64_t>(rte.texture_id));
            }
        }
    }
}

void RenderTargetCache::emit_passes(FrameContext& ctx, BatchBuilder& batch_builder,
                                    IRenderGraph& graph)
{
    if (!ctx.backend || !ctx.frame_buffer || !ctx.pipeline_map) {
        return;
    }

    if (!forward_path_) {
        forward_path_ = instance().create<IRenderPath>(ClassId::Path::Forward);
        if (!forward_path_) return;
    }

    // RTT subtrees render in Surface format regardless of which target
    // format the active camera path is using. Stash + restore the
    // FrameContext's format so per-view state set during the camera
    // loop isn't perturbed.
    PixelFormat saved_format = ctx.target_format;
    ctx.target_format = PixelFormat::Surface;

    for (auto& rtp : batch_builder.render_target_passes()) {
        auto it = entries_.find(rtp.element);
        if (it == entries_.end()) continue;
        auto& rte = it->second;
        if (!rte.target || rte.texture_id == 0) continue;

        FrameGlobals rt_globals{};
        build_ortho_projection(
            rt_globals.view_projection, static_cast<float>(rte.width), static_cast<float>(rte.height));
        rt_globals.viewport[0] = static_cast<float>(rte.width);
        rt_globals.viewport[1] = static_cast<float>(rte.height);
        rt_globals.viewport[2] = 1.0f / static_cast<float>(rte.width);
        rt_globals.viewport[3] = 1.0f / static_cast<float>(rte.height);
        rt_globals.bvh_root = ctx.bvh_root;
        rt_globals.bvh_node_count = ctx.bvh_node_count;
        rt_globals.bvh_shape_count = ctx.bvh_shape_count;
        rt_globals.bvh_nodes_addr = ctx.bvh_nodes_addr;
        rt_globals.bvh_shapes_addr = ctx.bvh_shapes_addr;
        uint64_t globals_gpu_addr = ctx.frame_buffer->write(&rt_globals, sizeof(rt_globals));

        RenderView rt_view{};
        rt_view.batches = &rtp.batches;
        rt_view.viewport = {0, 0,
                            static_cast<float>(rte.width),
                            static_cast<float>(rte.height)};
        rt_view.width = rte.width;
        rt_view.height = rte.height;
        rt_view.frame_globals_addr = globals_gpu_addr;
        rt_view.bvh_root = ctx.bvh_root;
        rt_view.bvh_node_count = ctx.bvh_node_count;
        rt_view.bvh_shape_count = ctx.bvh_shape_count;
        rt_view.bvh_nodes_addr = ctx.bvh_nodes_addr;
        rt_view.bvh_shapes_addr = ctx.bvh_shapes_addr;

        auto& entry = view_entries_[rtp.element];
        entry.cached_width = rte.width;
        entry.cached_height = rte.height;

        forward_path_->build_passes(entry, rt_view, rte.target, ctx, graph);
        rte.dirty = false;
    }

    ctx.target_format = saved_format;
}

void RenderTargetCache::on_element_removed(IElement* elem, FrameContext& ctx)
{
    auto rit = entries_.find(elem);
    if (rit == entries_.end()) {
        return;
    }
    if (rit->second.texture_id != 0 && ctx.resources) {
        ctx.resources->defer_texture_destroy(
            rit->second.texture_id, ctx.present_counter + ctx.latency_frames);
    }
    entries_.erase(rit);
}

void RenderTargetCache::shutdown(FrameContext& ctx)
{
    for (auto& [key, rte] : entries_) {
        if (rte.texture_id != 0 && ctx.backend) {
            ctx.backend->destroy_texture(rte.texture_id);
        }
    }
    entries_.clear();
}

} // namespace velk
