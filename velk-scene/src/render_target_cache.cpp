#include "render_target_cache.h"

#include "batch_builder.h"

#include <velk/api/state.h>
#include <velk/interface/intf_object_storage.h>

#include <velk-render/gpu_data.h>
#include <velk-render/interface/intf_render_backend.h>
#include <velk-scene/interface/intf_render_to_texture.h>

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

void RenderTargetCache::ensure(FrameContext& ctx)
{
    if (!ctx.backend || !ctx.batch_builder || !ctx.resources) {
        return;
    }
    for (auto& rtp : ctx.batch_builder->render_target_passes()) {
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
            tdesc.format = PixelFormat::RGBA8;
            tdesc.usage = TextureUsage::RenderTarget;
            rte.texture_id = ctx.backend->create_texture(tdesc);
            rte.width = w;
            rte.height = h;
            if (rte.texture_id != 0) {
                rte.target->set_render_target_id(static_cast<uint64_t>(rte.texture_id));
            }
        }
    }
}

void RenderTargetCache::emit_passes(FrameContext& ctx, vector<RenderPass>& out_passes)
{
    if (!ctx.backend || !ctx.frame_buffer || !ctx.batch_builder || !ctx.pipeline_map) {
        return;
    }

    for (auto& rtp : ctx.batch_builder->render_target_passes()) {
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

        vector<DrawCall> draw_calls;
        ctx.batch_builder->build_draw_calls(rtp.batches,
                                            draw_calls,
                                            *ctx.frame_buffer,
                                            *ctx.resources,
                                            globals_gpu_addr,
                                            ctx.pipeline_map,
                                            ctx.render_ctx,
                                            ctx.observer);

        RenderPass rt_pass{};
        rt_pass.target.target = rte.target;
        rt_pass.viewport = {0, 0, static_cast<float>(rte.width), static_cast<float>(rte.height)};
        rt_pass.draw_calls = std::move(draw_calls);
        out_passes.push_back(std::move(rt_pass));
        rte.dirty = false;
    }
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
