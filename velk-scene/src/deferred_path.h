#ifndef VELK_UI_DEFERRED_PATH_H
#define VELK_UI_DEFERRED_PATH_H

#include <velk/ext/core_object.h>
#include <velk/vector.h>

#include <unordered_map>

#include <velk-render/frustum.h>
#include <velk-render/interface/intf_camera.h>
#include <velk-scene/plugin.h>
#include <velk-scene/render_path/frame_context.h>
#include <velk-scene/render_path/intf_render_path.h>
#include <velk-scene/render_path/view_entry.h>

#include "batch_builder.h"

namespace velk {

/**
 * @brief Deferred shading path: G-buffer fill + compute lighting + blit.
 *
 * Two-stage pipeline collapsed into a single IRenderPath since one
 * camera attaches one path. Per view we:
 *
 *   1. Rebuild raster batches if the scene's visual set changed.
 *   2. Allocate / resize the per-view G-buffer (multi-attachment
 *      render target group) and emit a `PassKind::GBufferFill` pass
 *      that fills albedo / normal / world-pos / material-params.
 *   3. Allocate / resize the deferred output storage texture and the
 *      RT-shadow debug texture.
 *   4. Compose a compute pipeline for the active intersect-snippet
 *      set, look up G-buffer attachments, gather lights + env, and
 *      emit a `PassKind::ComputeBlit` pass that shades into the
 *      output texture and blits to the view surface.
 *
 * The two stages share per-view state (batches, gbuffer_group,
 * deferred_output_tex, shadow_debug_tex, frame_globals_addr) via the
 * `ViewState` struct.
 */
class DeferredPath : public ext::ObjectCore<DeferredPath, IRenderPath>
{
public:
    VELK_CLASS_UID(ClassId::Path::Deferred, "DeferredPath");

    void build_passes(ViewEntry& view,
                      const SceneState& scene_state,
                      FrameContext& ctx,
                      vector<RenderPass>& out_passes) override;

    void on_view_removed(ViewEntry& view, FrameContext& ctx) override;
    void shutdown(FrameContext& ctx) override;

    /// Lookup for Renderer::get_gbuffer_attachment.
    /// Returns 0 if no G-buffer has been allocated for this view yet.
    RenderTargetGroup find_gbuffer_group(ViewEntry* view) const;

    /// Lookup for Renderer::get_shadow_debug_texture.
    TextureId find_shadow_debug_tex(ViewEntry* view) const;

private:
    struct ViewState
    {
        vector<BatchBuilder::Batch> batches;

        RenderTargetGroup gbuffer_group = 0;
        int gbuffer_width = 0;
        int gbuffer_height = 0;

        TextureId deferred_output_tex = 0;
        int deferred_width = 0;
        int deferred_height = 0;

        TextureId shadow_debug_tex = 0;
        int shadow_debug_width = 0;
        int shadow_debug_height = 0;

        // Cross-stage: written by gbuffer-fill phase, read by lighter phase
        // within the same build_passes call.
        uint64_t frame_globals_addr = 0;
    };

    std::unordered_map<ViewEntry*, ViewState> view_states_;

    /// Compiled compute pipelines keyed by FNV hash of active intersect
    /// id set; each variant compiles once, kept across frames.
    std::unordered_map<uint64_t, bool> compiled_pipelines_;

    uint64_t ensure_pipeline(FrameContext& ctx);

    RenderTargetGroup ensure_gbuffer(ViewState& vs, int width, int height,
                                     FrameContext& ctx);

    void emit_gbuffer_pass(ViewEntry& view, ViewState& vs, FrameContext& ctx,
                           uint64_t globals_gpu_addr,
                           const ::velk::render::Frustum* frustum,
                           vector<RenderPass>& out_passes);

    void emit_lighting_pass(ViewEntry& view, ViewState& vs,
                            const SceneState& scene_state, FrameContext& ctx,
                            int w, int h,
                            vector<RenderPass>& out_passes);
};

} // namespace velk

#endif // VELK_UI_DEFERRED_PATH_H
