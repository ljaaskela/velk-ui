#ifndef VELK_RENDER_INTF_RENDER_GRAPH_INTERNAL_H
#define VELK_RENDER_INTF_RENDER_GRAPH_INTERNAL_H

#include <velk-render/interface/intf_render_backend.h>
#include <velk-render/interface/intf_render_graph.h>

namespace velk {

/**
 * @brief Framework-internal lifecycle API for the render graph.
 *
 * Pipelines / paths see only `IRenderGraph` (add_pass, compile,
 * execute, resources). The Renderer drives backend wiring via this
 * sibling interface so the public surface stays minimal.
 */
class IRenderGraphInternal
    : public Interface<IRenderGraphInternal, IRenderGraph>
{
public:
    /// Wires the graph to a backend. Called by Renderer right after
    /// the graph is created. The graph propagates the backend to its
    /// internally-owned transient `IGpuResourceManager`, which uses it
    /// to create per-frame intermediate textures, groups, etc.
    virtual void init(IRenderBackend* backend) = 0;
};

} // namespace velk

#endif // VELK_RENDER_INTF_RENDER_GRAPH_INTERNAL_H
