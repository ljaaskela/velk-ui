#include "render_graph.h"

namespace velk::impl {

void RenderGraph::clear()
{
    passes_.clear();
    barriers_.clear();
    states_.clear();
    imported_.clear();
}

void RenderGraph::import(const ::velk::IGpuResource::Ptr& resource)
{
    if (!resource) return;
    auto* raw = resource.get();
    imported_[raw] = true;
    states_.emplace(raw, ResourceState::Undefined);
}

void RenderGraph::note_resource(const ::velk::IGpuResource::Ptr& resource)
{
    if (!resource) return;
    states_.emplace(resource.get(), ResourceState::Undefined);
}

RenderGraph::ResourceState
RenderGraph::write_state_for(const ::velk::RenderPass& body)
{
    switch (body.kind) {
    case ::velk::PassKind::Raster:
    case ::velk::PassKind::GBufferFill:
    case ::velk::PassKind::ComputeBlit:
    case ::velk::PassKind::Blit:
        return ResourceState::ColorWrite;
    case ::velk::PassKind::Compute:
        return ResourceState::Storage;
    }
    return ResourceState::ColorWrite;
}

void RenderGraph::add_pass(::velk::GraphPass&& pass)
{
    if (pass.writes.empty() && pass.body.target.target) {
        pass.writes.push_back(pass.body.target.target);
    }
    for (auto& r : pass.reads)  note_resource(r);
    for (auto& w : pass.writes) note_resource(w);
    passes_.push_back(std::move(pass));
}

void RenderGraph::add_pass(::velk::RenderPass&& body)
{
    ::velk::GraphPass gp;
    gp.body = std::move(body);
    add_pass(std::move(gp));
}

void RenderGraph::compile()
{
    barriers_.assign(passes_.size(), Barrier{});

    /// Tier 1 conservative state machine. Mirrors the heuristic the
    /// old `Renderer::present()` loop used (`had_texture_pass`):
    /// GBufferFill / Raster-into-texture leaves the target in
    /// ColorWrite; the next consumer needs a barrier to its stage
    /// (Compute or Fragment). Reads are inferred from pass kind in
    /// Tier 1; Tier 2 will drive transitions from declared reads/writes.
    bool had_texture_pass = false;
    for (size_t i = 0; i < passes_.size(); ++i) {
        auto& gp = passes_[i];
        auto& barrier = barriers_[i];

        bool is_compute_consumer =
            gp.body.kind == ::velk::PassKind::ComputeBlit ||
            gp.body.kind == ::velk::PassKind::Compute ||
            gp.body.kind == ::velk::PassKind::Blit;
        bool is_fragment_consumer = gp.body.kind == ::velk::PassKind::GBufferFill;
        bool is_raster = gp.body.kind == ::velk::PassKind::Raster;
        bool raster_to_texture = false;
        if (is_raster) {
            auto* rt = gp.body.target.target.get();
            raster_to_texture = rt && rt->get_type() == ::velk::GpuResourceType::Texture;
        }

        if (had_texture_pass) {
            if (is_compute_consumer) {
                barrier.emit = true;
                barrier.src = ::velk::PipelineStage::ColorOutput;
                barrier.dst = ::velk::PipelineStage::ComputeShader;
                had_texture_pass = false;
            } else if (is_fragment_consumer ||
                       (is_raster && !raster_to_texture)) {
                barrier.emit = true;
                barrier.src = ::velk::PipelineStage::ColorOutput;
                barrier.dst = ::velk::PipelineStage::FragmentShader;
                had_texture_pass = false;
            }
        }

        if (gp.body.kind == ::velk::PassKind::GBufferFill || raster_to_texture) {
            had_texture_pass = true;
        }

        for (auto& w : gp.writes) {
            states_[w.get()] = write_state_for(gp.body);
        }
    }
}

void RenderGraph::execute(::velk::IRenderBackend& backend)
{
    for (size_t i = 0; i < passes_.size(); ++i) {
        auto& gp = passes_[i];
        auto& barrier = barriers_[i];
        auto& pass = gp.body;

        if (barrier.emit) {
            backend.barrier(barrier.src, barrier.dst);
        }

        switch (pass.kind) {
        case ::velk::PassKind::ComputeBlit:
            backend.dispatch({&pass.compute, 1});
            backend.blit_to_surface(pass.blit_source, pass.blit_surface_id,
                                    pass.blit_dst_rect);
            if (pass.blit_depth_source_group != 0) {
                backend.blit_group_depth_to_surface(pass.blit_depth_source_group,
                                                    pass.blit_surface_id,
                                                    pass.blit_dst_rect);
            }
            break;

        case ::velk::PassKind::Blit:
            backend.blit_to_surface(pass.blit_source, pass.blit_surface_id,
                                    pass.blit_dst_rect);
            break;

        case ::velk::PassKind::Compute:
            backend.dispatch({&pass.compute, 1});
            break;

        case ::velk::PassKind::GBufferFill:
            backend.begin_pass(pass.gbuffer_group);
            backend.submit({pass.draw_calls.data(), pass.draw_calls.size()},
                           pass.viewport);
            backend.end_pass();
            break;

        case ::velk::PassKind::Raster: {
            auto* rt = pass.target.target.get();
            uint64_t pass_target_id = rt ? rt->get_render_target_id() : 0;
            backend.begin_pass(pass_target_id);
            backend.submit({pass.draw_calls.data(), pass.draw_calls.size()},
                           pass.viewport);
            backend.end_pass();
            break;
        }
        }
    }
}

} // namespace velk::impl
