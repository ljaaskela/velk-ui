#ifndef VELK_UI_BATCH_BUILDER_H
#define VELK_UI_BATCH_BUILDER_H

#include <velk/vector.h>

#include <unordered_map>
#include <velk-render/frame/batch.h>
#include <velk-render/frame/draw_call_emit.h>
#include <velk-render/interface/intf_frame_data_manager.h>
#include <velk-render/interface/intf_gpu_resource_manager.h>
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


    struct RenderTargetPassData
    {
        IElement* element = nullptr;
        vector<IElement*> before_entries;  // pre-order
        vector<IElement*> after_entries;   // post-order
        vector<Batch> batches;
    };

    /**
     * @brief Rebuilds visual commands for a single element from its traits.
     *
     * Captures shader sources, pipeline options, and material textures
     * onto the cached visual / draw entries. Pipeline compilation is
     * deferred to `IRenderContext::build_draw_calls`, which compiles
     * lazily against the active path's `FrameContext::target_format`.
     */
    void rebuild_commands(IElement* element, IGpuResourceObserver* observer,
                          IRenderContext* render_ctx);

    /** @brief Rebuilds batches from the visual list, pre-filtering render target subtrees. */
    void rebuild_batches(const SceneState& state, vector<Batch>& out_batches);

    /** @brief Removes an element from the cache. */
    void evict(IElement* element) { element_cache_.erase(element); }

    /** @brief Clears all cached data. */
    void clear() { element_cache_.clear(); render_target_passes_.clear(); }

    /**
     * @brief Clears the render-target-pass union so each view's
     *        `rebuild_batches` this frame accumulates into a fresh
     *        list. Material upload dedup cache lives on Renderer now.
     */
    void reset_frame_state()
    {
        render_target_passes_.clear();
    }

    /** @brief Returns the element cache (for resource upload iteration). */
    const std::unordered_map<IElement*, ElementCache>& element_cache() const { return element_cache_; }

    /** @brief Returns the render target passes collected during rebuild_batches. */
    const vector<RenderTargetPassData>& render_target_passes() const { return render_target_passes_; }

    /** @brief Mutable access to render target passes (for moving batches in prepare). */
    vector<RenderTargetPassData>& render_target_passes() { return render_target_passes_; }

private:
    std::unordered_map<IElement*, ElementCache> element_cache_;
    vector<RenderTargetPassData> render_target_passes_;
};

} // namespace velk

#endif // VELK_UI_BATCH_BUILDER_H
