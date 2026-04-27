#ifndef VELK_SCENE_RENDER_PATH_FRAME_CONTEXT_H
#define VELK_SCENE_RENDER_PATH_FRAME_CONTEXT_H

#include <unordered_map>

#include <velk-render/frame/intf_frame_data_manager.h>
#include <velk-render/frame/intf_frame_snippet_registry.h>
#include <velk-render/frame/intf_gpu_resource_manager.h>
#include <velk-render/interface/intf_gpu_resource.h>
#include <velk-render/interface/intf_render_backend.h>
#include <velk-render/interface/intf_render_context.h>
#include <velk-render/render_types.h>

namespace velk {

// Forward declarations of velk-scene-internal helpers carried by
// FrameContext. Built-in raster paths in velk-scene/src include the
// concrete headers; external IRenderPath implementations that don't
// need scene-internal batching ignore these fields.
class BatchBuilder;
class RenderTargetCache;

/**
 * @brief Shared non-owning context passed to per-view render paths.
 *
 * Bundles every dependency a path needs to upload data and emit GPU
 * passes. velk-render-side fields (backend, render_ctx, frame_buffer,
 * resources, snippets, pipeline_map, observer) work for any render
 * path. velk-scene-internal fields (batch_builder, render_target_cache)
 * are usable only by paths in velk-scene that include the corresponding
 * private headers; external paths can leave them alone.
 */
struct FrameContext
{
    IRenderBackend* backend = nullptr;
    IRenderContext* render_ctx = nullptr;
    IFrameDataManager* frame_buffer = nullptr;
    IGpuResourceManager* resources = nullptr;
    IFrameSnippetRegistry* snippets = nullptr;
    BatchBuilder* batch_builder = nullptr;
    RenderTargetCache* render_target_cache = nullptr;
    const std::unordered_map<uint64_t, PipelineId>* pipeline_map = nullptr;
    IGpuResourceObserver* observer = nullptr;
    uint64_t present_counter = 0;
    uint64_t latency_frames = 0;

    // Scene-wide BVH built once per frame in build_frame_passes before
    // any view renders; consumed by paths when they stamp out
    // FrameGlobals. Zero when the view's scene has no BVH.
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

} // namespace velk

#endif // VELK_SCENE_RENDER_PATH_FRAME_CONTEXT_H
