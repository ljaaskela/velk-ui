#ifndef VELK_RENDER_RENDER_GRAPH_H
#define VELK_RENDER_RENDER_GRAPH_H

#include <velk/ext/core_object.h>

#include <unordered_map>

#include <velk-render/frame/intf_render_graph.h>
#include <velk-render/plugin.h>

namespace velk::impl {

/**
 * @brief Tier 1 RenderGraph implementation.
 *
 * Insertion-ordered. Compile assigns pre-pass barriers using a
 * conservative state machine that mirrors the old
 * `Renderer::present()` `had_texture_pass` heuristic. Execute walks
 * compiled passes and dispatches per `PassKind` to the backend.
 */
class RenderGraph final
    : public ::velk::ext::ObjectCore<RenderGraph, ::velk::IRenderGraph>
{
public:
    VELK_CLASS_UID(::velk::ClassId::RenderGraph, "RenderGraph");

    bool empty() const override { return passes_.empty(); }
    void clear() override;
    void import(const ::velk::IGpuResource::Ptr& resource) override;
    void add_pass(::velk::GraphPass&& pass) override;
    void add_pass(::velk::RenderPass&& body) override;
    void compile() override;
    void execute(::velk::IRenderBackend& backend) override;

    ::velk::vector<::velk::GraphPass>& passes() override { return passes_; }
    const ::velk::vector<::velk::GraphPass>& passes() const override { return passes_; }

private:
    enum class ResourceState : uint8_t
    {
        Undefined,
        ColorWrite,
        DepthWrite,
        ShaderRead,
        Storage,
        Present,
    };

    struct Barrier
    {
        bool emit = false;
        ::velk::PipelineStage src = ::velk::PipelineStage::ColorOutput;
        ::velk::PipelineStage dst = ::velk::PipelineStage::FragmentShader;
    };

    ::velk::vector<::velk::GraphPass> passes_;
    ::velk::vector<Barrier> barriers_;
    std::unordered_map<::velk::IGpuResource*, ResourceState> states_;
    std::unordered_map<::velk::IGpuResource*, bool> imported_;

    void note_resource(const ::velk::IGpuResource::Ptr& resource);
    static ResourceState write_state_for(const ::velk::RenderPass& body);
};

} // namespace velk::impl

#endif // VELK_RENDER_RENDER_GRAPH_H
