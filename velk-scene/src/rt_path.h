#ifndef VELK_UI_RT_PATH_H
#define VELK_UI_RT_PATH_H

#include <velk/ext/core_object.h>
#include <velk/string.h>
#include <velk/vector.h>

#include <unordered_map>

#include <velk-scene/plugin.h>
#include <velk-scene/render_path/frame_context.h>
#include <velk-scene/render_path/intf_render_path.h>
#include <velk-scene/render_path/view_entry.h>

namespace velk {

class IShadowTechnique;

/**
 * @brief Compute-shader path tracer render path.
 *
 * Allocates a per-view storage output texture, builds a flat painter-
 * sorted shape buffer, dispatches a composed compute shader against
 * the scene-wide BVH, and blits the shaded output to the surface.
 *
 * Owns the compiled compute pipeline cache (keyed by active material
 * + shadow-tech + intersect snippet sets) and per-view RT allocations.
 */
class RtPath : public ext::ObjectCore<RtPath, IRenderPath>
{
public:
    VELK_CLASS_UID(ClassId::Path::Rt, "RtPath");

    void build_passes(ViewEntry& view,
                      const SceneState& scene_state,
                      const RenderView& render_view,
                      FrameContext& ctx,
                      vector<RenderPass>& out_passes) override;
    void on_view_removed(ViewEntry& view, FrameContext& ctx) override;
    void shutdown(FrameContext& ctx) override;

private:
    struct ViewState
    {
        TextureId rt_output_tex = 0;
        int width = 0;
        int height = 0;
    };

    std::unordered_map<ViewEntry*, ViewState> view_states_;

    /// Compiled compute pipelines keyed by FNV hash of (materials,
    /// shadow techs, intersects). Composition runs through the shared
    /// FrameSnippetRegistry on ctx.
    std::unordered_map<uint64_t, bool> compiled_pipelines_;

    uint64_t ensure_pipeline(FrameContext& ctx);
};

} // namespace velk

#endif // VELK_UI_RT_PATH_H
