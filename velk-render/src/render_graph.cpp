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
    states_.clear();

    /// Per-resource state machine driving barriers. Each pass:
    ///  1. If it reads textures (any consumer pass except RTT-style
    ///     Raster-into-texture, which is assumed to write fresh content
    ///     without sampling previously-written graph resources),
    ///     scans the state map for resources in writeable states
    ///     (ColorWrite / Storage), emits a single transition barrier
    ///     from the producer stage to the consumer stage, then marks
    ///     those resources as ShaderRead.
    ///  2. Applies its declared writes, transitioning each written
    ///     resource into the new state implied by the pass kind.
    ///
    /// Bindless texture reads from materials are NOT declared on the
    /// pass; they're caught by step (1)'s scan because the producing
    /// resource (RTT, gbuffer attachment, deferred output) is in the
    /// state map after its write. Tier 2 will require explicit reads
    /// + per-Ptr barrier emission once transient pooling needs them.
    auto consumer_stage = [](::velk::PassKind k) {
        switch (k) {
        case ::velk::PassKind::Raster:
        case ::velk::PassKind::GBufferFill:
            return ::velk::PipelineStage::FragmentShader;
        case ::velk::PassKind::Compute:
        case ::velk::PassKind::ComputeBlit:
        case ::velk::PassKind::Blit:
            return ::velk::PipelineStage::ComputeShader;
        }
        return ::velk::PipelineStage::FragmentShader;
    };

    auto reads_textures = [](::velk::PassKind k, bool raster_to_texture) {
        // Raster-to-texture (RTT) passes write a fresh target without
        // sampling previously-written graph resources; skip the
        // pre-pass barrier for them. Every other pass kind samples
        // bindless textures or storage images and needs the barrier
        // when prior writes are in flight.
        if (k == ::velk::PassKind::Raster) return !raster_to_texture;
        return true;
    };

    for (size_t i = 0; i < passes_.size(); ++i) {
        auto& gp = passes_[i];
        auto& barrier = barriers_[i];

        bool raster_to_texture = false;
        if (gp.body.kind == ::velk::PassKind::Raster) {
            auto* rt = gp.body.target.target.get();
            raster_to_texture = rt && rt->get_type() == ::velk::GpuResourceType::Texture;
        }

        if (reads_textures(gp.body.kind, raster_to_texture)) {
            ::velk::PipelineStage src_stage = ::velk::PipelineStage::ColorOutput;
            bool need_barrier = false;
            for (auto& [r, st] : states_) {
                if (st == ResourceState::ColorWrite) {
                    src_stage = ::velk::PipelineStage::ColorOutput;
                    need_barrier = true;
                } else if (st == ResourceState::Storage) {
                    src_stage = ::velk::PipelineStage::ComputeShader;
                    need_barrier = true;
                }
            }
            if (need_barrier) {
                barrier.emit = true;
                barrier.src = src_stage;
                barrier.dst = consumer_stage(gp.body.kind);
                for (auto& [r, st] : states_) {
                    if (st == ResourceState::ColorWrite || st == ResourceState::Storage) {
                        st = ResourceState::ShaderRead;
                    }
                }
            }
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
            uint64_t pass_target_id = rt ? rt->get_gpu_handle(GpuResourceKey::Default) : 0;
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
