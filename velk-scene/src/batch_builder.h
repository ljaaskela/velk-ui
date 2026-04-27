#ifndef VELK_UI_BATCH_BUILDER_H
#define VELK_UI_BATCH_BUILDER_H

#include <velk/vector.h>

#include <unordered_map>
#include <velk-render/frame/batch.h>
#include <velk-render/frame/draw_call_emit.h>
#include <velk-render/frame/intf_frame_data_manager.h>
#include <velk-render/frame/intf_gpu_resource_manager.h>
#include <velk-render/frustum.h>
#include <velk-render/interface/intf_buffer.h>
#include <velk-render/interface/intf_mesh.h>
#include <velk-render/interface/intf_program.h>
#include <velk-render/interface/intf_raster_shader.h>
#include <velk-render/interface/intf_render_backend.h>
#include <velk-render/interface/intf_render_context.h>
#include <velk-render/interface/intf_render_target.h>
#include <velk-render/interface/intf_shader_snippet.h>
#include <velk-render/interface/intf_texture_resolver.h>
#include <velk-render/render_types.h>
#include <velk-scene/interface/intf_element.h>
#include <velk-scene/interface/intf_scene.h>

namespace velk {

/**
 * @brief Builds draw batches from the visual list and converts them to GPU draw calls.
 *
 * Manages the element visual cache, pre-filters render target subtrees,
 * and assembles draw calls into the frame data buffer.
 */
class BatchBuilder
{
public:
    struct VisualCommands
    {
        vector<DrawEntry> entries;
        // Per-visual deferred-fragment discard snippet (SDF corners,
        // glyph coverage, ...). Non-null only when the visual opts in
        // via IShaderSnippet. See batch_builder's gbuffer pipeline
        // composer for how it's spliced into the material's fragment.
        // `discard_key_perturb` is a precomputed 64-bit contribution
        // XORed into the gbuffer pipeline cache key so two visuals that
        // share a material still get distinct composed pipelines.
        IShaderSnippet::Ptr visual_discard;
        uint64_t discard_key_perturb = 0;
        // The visual's IRasterShader, cached so the deferred gbuffer
        // pipeline compose can pull its vertex shader when no material
        // is routed (e.g. primitive mesh visuals).
        IRasterShader::Ptr raster_shader;
        // Materials are per-entry (see DrawEntry::material). Pipeline
        // compilation runs once per unique material during rebuild_commands
        // and the resulting handle is stashed on each entry's
        // pipeline_override.
    };

    struct ElementCache
    {
        vector<VisualCommands> before_visuals;
        vector<VisualCommands> after_visuals;
        vector<IBuffer::WeakPtr> gpu_resources;
    };

    /// `Batch` is now the velk-render-public `velk::Batch`. Aliased here
    /// for source compatibility while the rest of velk-scene's batching
    /// logic still references it as `BatchBuilder::Batch`.
    using Batch = ::velk::Batch;

    struct RenderTargetPassData
    {
        IElement* element = nullptr;
        vector<IElement*> before_entries;  // pre-order
        vector<IElement*> after_entries;   // post-order
        vector<Batch> batches;
    };

    /** @brief Rebuilds visual commands for a single element from its traits. */
    void rebuild_commands(IElement* element, IGpuResourceObserver* observer, IRenderContext* render_ctx);

    /** @brief Rebuilds batches from the visual list, pre-filtering render target subtrees. */
    void rebuild_batches(const SceneState& state, vector<Batch>& out_batches);

    /**
     * @brief Same as `velk::build_draw_calls` (in velk-render), but
     *        emits deferred-pipeline draw calls targeting a G-buffer
     *        render target group.
     *
     * Compiles G-buffer pipeline variants on demand (one per forward
     * pipeline_key) using the material's `get_gbuffer_*_src()` when a
     * material is present, otherwise the registered default G-buffer
     * shaders. Variants are cached in `render_ctx->gbuffer_pipeline_map()`
     * and reused across views (group render passes are format-compatible).
     */
    void build_gbuffer_draw_calls(const vector<Batch>& batches, vector<DrawCall>& out_calls,
                                  IFrameDataManager& frame_data, IGpuResourceManager& resources,
                                  uint64_t globals_gpu_addr,
                                  IRenderContext* render_ctx,
                                  RenderTargetGroup target_group,
                                  IGpuResourceObserver* observer,
                                  const ::velk::render::Frustum* frustum = nullptr);

    /** @brief Removes an element from the cache. */
    void evict(IElement* element) { element_cache_.erase(element); }

    /** @brief Clears all cached data. */
    void clear() { element_cache_.clear(); render_target_passes_.clear(); }

    /**
     * @brief Resets per-frame state. Clears the material address cache
     *        (used by both `velk::build_draw_calls` and the in-class
     *        `build_gbuffer_draw_calls`) so each frame uploads material
     *        params once regardless of how many batches reference the
     *        same material. Also clears the render-target-pass union
     *        so each view's `rebuild_batches` this frame accumulates
     *        into a fresh list.
     */
    void reset_frame_state()
    {
        material_addr_cache_.clear();
        render_target_passes_.clear();
    }

    /// Per-frame `IProgram*->mat_addr` cache. Shared with the
    /// `velk::build_draw_calls` free function via FrameContext so
    /// forward + deferred paths in one frame don't double-upload.
    MaterialAddrCache& material_addr_cache() { return material_addr_cache_; }

    /** @brief Returns the element cache (for resource upload iteration). */
    const std::unordered_map<IElement*, ElementCache>& element_cache() const { return element_cache_; }

    /** @brief Returns the render target passes collected during rebuild_batches. */
    const vector<RenderTargetPassData>& render_target_passes() const { return render_target_passes_; }

    /** @brief Mutable access to render target passes (for moving batches in prepare). */
    vector<RenderTargetPassData>& render_target_passes() { return render_target_passes_; }

private:
    std::unordered_map<IElement*, ElementCache> element_cache_;
    vector<RenderTargetPassData> render_target_passes_;
    MaterialAddrCache material_addr_cache_;
};

} // namespace velk

#endif // VELK_UI_BATCH_BUILDER_H
