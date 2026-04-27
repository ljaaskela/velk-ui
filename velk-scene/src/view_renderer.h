#ifndef VELK_UI_VIEW_RENDERER_H
#define VELK_UI_VIEW_RENDERER_H

#include <velk/vector.h>

#include <unordered_map>

#include <velk-render/frame/intf_frame_data_manager.h>
#include <velk-render/frame/intf_frame_snippet_registry.h>
#include <velk-render/frame/intf_gpu_resource_manager.h>
#include <velk-render/frame/render_pass.h>
#include <velk-render/interface/intf_gpu_resource.h>
#include <velk-render/interface/intf_render_backend.h>
#include <velk-render/interface/intf_render_context.h>
#include <velk-render/interface/intf_window_surface.h>
#include <velk-render/render_types.h>
#include <velk-scene/interface/intf_element.h>
#include <velk-scene/interface/intf_scene.h>

#include "batch_builder.h"

namespace velk {

class RenderTargetCache;

/** @brief Shared non-owning context passed to per-view sub-renderers. */
struct FrameContext
{
    IRenderBackend* backend = nullptr;
    IRenderContext* render_ctx = nullptr;
    IFrameDataManager* frame_buffer = nullptr;
    IGpuResourceManager* resources = nullptr;
    BatchBuilder* batch_builder = nullptr; ///< Shared visual-command cache.
    IFrameSnippetRegistry* snippets = nullptr; ///< Shared material / shadow-tech / intersect snippet registry.
    RenderTargetCache* render_target_cache = nullptr; ///< Shared RTT (render-to-texture) subtree cache.
    const std::unordered_map<uint64_t, PipelineId>* pipeline_map = nullptr;
    IGpuResourceObserver* observer = nullptr; ///< Renderer as observer of GPU resources.
    uint64_t present_counter = 0;
    uint64_t latency_frames = 0;

    // Scene-wide BVH built once per frame in build_frame_passes before
    // any view renders; consumed by sub-renderers when they stamp out
    // FrameGlobals. Zero when the view's scene has no BVH (or BVH build
    // failed). See scene_collector::build_scene_bvh.
    uint64_t bvh_nodes_addr = 0;
    uint64_t bvh_shapes_addr = 0;
    uint32_t bvh_root = 0;
    uint32_t bvh_node_count = 0;
    uint32_t bvh_shape_count = 0;

    /// Convenience: assemble a FrameResolveContext for snippet-registry calls.
    FrameResolveContext make_resolve_context() const
    {
        return {render_ctx, resources, frame_buffer, present_counter + latency_frames};
    }
};

/**
 * @brief Renderer-owned per-view state shared across all paths.
 */
struct ViewEntry
{
    IElement::Ptr camera_element;
    IWindowSurface::Ptr surface;
    rect viewport;

    // Raster batch cache (Forward + Deferred G-buffer paths).
    bool batches_dirty = true;
    int cached_width = 0;
    int cached_height = 0;
    vector<BatchBuilder::Batch> batches;

    // Transient: GPU address of this view's FrameGlobals block for the
    // current frame. Written by the raster path that runs first (Forward
    // for Forward views; DeferredGBufferPath for Deferred views) and
    // read by DeferredLighter so its compute shader can reach scene-wide
    // state (inv_vp, BVH) via the canonical GlobalData layout.
    uint64_t frame_globals_addr = 0;
};

/**
 * @brief Interface for a per-view renderer.
 *
 * TODO(plugin-view-renderers): Phase 2.2 promotes this to a public
 * `IRenderPath` (Interface<IRenderPath>) attached to the camera element,
 * with ViewEntry / FrameContext moved to public velk-scene headers.
 */
class IViewRenderer
{
public:
    virtual ~IViewRenderer() = default;

    virtual void build_passes(ViewEntry& view,
                              const SceneState& scene_state,
                              FrameContext& ctx,
                              vector<RenderPass>& out_passes) = 0;

    virtual void build_shared_passes(FrameContext& /*ctx*/,
                                     vector<RenderPass>& /*out_passes*/) {}

    virtual void on_view_removed(ViewEntry& /*view*/, FrameContext& /*ctx*/) {}
    virtual void on_element_removed(IElement* /*elem*/, FrameContext& /*ctx*/) {}
    virtual void shutdown(FrameContext& /*ctx*/) {}
};

} // namespace velk

#endif // VELK_UI_VIEW_RENDERER_H
