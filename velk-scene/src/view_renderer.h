#ifndef VELK_UI_VIEW_RENDERER_H
#define VELK_UI_VIEW_RENDERER_H

#include <velk/vector.h>

#include <unordered_map>
#include <velk-render/interface/intf_gpu_resource.h>
#include <velk-render/interface/intf_render_backend.h>
#include <velk-render/interface/intf_render_context.h>
#include <velk-render/interface/intf_render_target.h>
#include <velk-render/interface/intf_window_surface.h>
#include <velk-render/render_types.h>
#include <velk-scene/interface/intf_element.h>
#include <velk-scene/interface/intf_scene.h>

#include "batch_builder.h"
#include "frame_data_manager.h"
#include "gpu_resource_manager.h"

namespace velk {

class FrameSnippetRegistry;
class RenderTargetCache;

/** @brief Shared non-owning context passed to per-view sub-renderers. */
struct FrameContext
{
    IRenderBackend* backend = nullptr;
    IRenderContext* render_ctx = nullptr;
    FrameDataManager* frame_buffer = nullptr;
    GpuResourceManager* resources = nullptr;
    BatchBuilder* batch_builder = nullptr; ///< Shared visual-command cache.
    FrameSnippetRegistry* snippets = nullptr; ///< Shared material / shadow-tech / intersect snippet registry.
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
};

/** @brief Bundles a render target owned by a pass. */
struct ViewRenderTarget
{
    IRenderTarget::Ptr target;
};

/** @brief Discriminator for the kind of pass a sub-renderer produces. */
enum class PassKind
{
    Raster,       ///< Classic vkCmdBeginRenderPass + draw calls against a surface or RTT texture.
    ComputeBlit,  ///< Compute dispatch writing to a storage image, then blit to a surface.
    GBufferFill,  ///< Raster draws into a multi-attachment G-buffer group (no surface blit).
    Compute,      ///< Pure compute dispatch, no blit. Result consumed by a later pass via sampled image / storage.
    Blit,         ///< Pure blit from a source texture to a surface rect. Used for debug overlays.
};

/**
 * @brief A unit of GPU work produced by a sub-renderer and submitted by
 *        Renderer in a single command buffer alongside the other passes.
 *
 * The structure is a tagged union (see PassKind). Unused fields for the
 * non-active kind are zero-initialised.
 */
struct RenderPass
{
    PassKind kind = PassKind::Raster;

    ViewRenderTarget target;
    rect viewport;
    vector<DrawCall> draw_calls;

    RenderTargetGroup gbuffer_group = 0;

    DispatchCall compute{};
    uint64_t blit_surface_id = 0;
    TextureId blit_source = 0;
    rect blit_dst_rect{};

    RenderTargetGroup blit_depth_source_group = 0;
};

/**
 * @brief Renderer-owned per-view state shared across all paths.
 *
 * Carries identity (camera_element, surface, viewport), the raster batch
 * cache (consumed by both Forward and Deferred G-buffer paths), and the
 * one transient that has to flow between paths in a single frame
 * (frame_globals_addr, written by the raster path that runs first and
 * read by DeferredLighter). Path-specific resources (RT output texture,
 * G-buffer group, deferred / shadow-debug storage textures) live in
 * per-path ViewEntry* -> state maps owned by each IViewRenderer.
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
 * An IViewRenderer knows how to turn a single view (camera + surface +
 * viewport + scene state) into zero or more RenderPasses. The outer Renderer
 * dispatches each view to one or more sub-renderers based on the camera's
 * `render_path` property.
 *
 * TODO(plugin-view-renderers): Phase 2 promotes this to a public
 * `IRenderPath` (Interface<IRenderPath, IShaderSnippet>) attached to the
 * camera element, with ViewEntry / FrameContext / RenderPass moved to
 * public velk-scene headers and the GPU plumbing managers
 * (GpuResourceManager, FrameDataManager, FrameSnippetRegistry) promoted
 * to interfaces in velk-render. See project_render_path_seam memory.
 */
class IViewRenderer
{
public:
    virtual ~IViewRenderer() = default;

    /**
     * @brief Appends zero or more passes for @p view to @p out_passes.
     */
    virtual void build_passes(ViewEntry& view,
                              const SceneState& scene_state,
                              FrameContext& ctx,
                              vector<RenderPass>& out_passes) = 0;

    /**
     * @brief Emits any extra passes that are not per-view (e.g. raster
     *        render-to-texture passes that serve multiple views).
     */
    virtual void build_shared_passes(FrameContext& /*ctx*/,
                                     vector<RenderPass>& /*out_passes*/) {}

    /** @brief Hook called when a view is removed. Release per-view state. */
    virtual void on_view_removed(ViewEntry& /*view*/, FrameContext& /*ctx*/) {}

    /**
     * @brief Hook called when an element is detached from a scene.
     */
    virtual void on_element_removed(IElement* /*elem*/, FrameContext& /*ctx*/) {}

    /** @brief Hook called on Renderer shutdown. Release all remaining state. */
    virtual void shutdown(FrameContext& /*ctx*/) {}
};

} // namespace velk

#endif // VELK_UI_VIEW_RENDERER_H
