#ifndef VELK_UI_RENDERER_IMPL_H
#define VELK_UI_RENDERER_IMPL_H

#include <velk/ext/object.h>
#include <velk/vector.h>

#include <unordered_map>
#include <unordered_set>
#include "batch_builder.h"
#include "frame_data_manager.h"
#include "frame_snippet_registry.h"
#include "gpu_resource_manager.h"
#include "render_target_cache.h"
#include <velk-scene/render_path/frame_context.h>
#include <velk-scene/render_path/intf_render_path.h>
#include <velk-scene/render_path/view_entry.h>
#include <velk-render/detail/intf_renderer_internal.h>
#include <velk-render/gpu_data.h>
#include <velk-render/interface/intf_gpu_resource.h>
#include <velk-render/interface/intf_render_backend.h>
#include <velk-render/interface/intf_render_context.h>
#include <velk-render/interface/intf_render_target.h>
#include <velk-render/interface/intf_window_surface.h>
#include <velk-render/plugin.h>
#include <velk-render/render_types.h>
#include <velk-render/interface/intf_camera.h>
#include <velk-scene/interface/intf_render_to_texture.h>
#include <velk-scene/interface/intf_renderer.h>
#include <velk-scene/interface/intf_scene.h>
#include "scene_bvh.h"

#include <condition_variable>
#include <mutex>

namespace velk {

class Renderer : public ::velk::ext::Object<Renderer, IRendererInternal, IRenderer, IGpuResourceObserver>
{
public:
    VELK_CLASS_UID(::velk::ClassId::Renderer, "Renderer");

    // IRendererInternal
    void set_backend(const IRenderBackend::Ptr& backend, IRenderContext* ctx) override;

    // IGpuResourceObserver
    void on_gpu_resource_destroyed(IGpuResource* resource) override;

    // IRenderer
    void add_view(const IElement::Ptr& camera_element, const IWindowSurface::Ptr& surface,
                  const rect& viewport) override;
    void remove_view(const IElement::Ptr& camera_element, const IWindowSurface::Ptr& surface) override;
    Frame prepare(const FrameDesc& desc) override;
    void present(Frame frame) override;
    void render() override;
    void set_max_frames_in_flight(uint32_t count) override;
    void add_debug_overlay(const IWindowSurface::Ptr& surface,
                           TextureId texture_id,
                           const rect& dst_rect) override;
    void clear_debug_overlays() override;
    TextureId get_gbuffer_attachment(const IElement::Ptr& camera_element,
                                     const IWindowSurface::Ptr& surface,
                                     uint32_t attachment_index) const override;
    TextureId get_shadow_debug_texture(const IElement::Ptr& camera_element,
                                       const IWindowSurface::Ptr& surface) const override;
    void request_bvh_log() override { log_bvh_next_ = true; }
    void shutdown() override;

private:
    struct FrameSlot
    {
        uint64_t id = 0;
        vector<RenderPass> passes;
        bool ready = false;
        uint64_t presented_at = 0;
        IFrameDataManager::Slot buffer;
    };

    FrameSlot* claim_frame_slot();
    std::unordered_map<IScene*, SceneState> consume_scenes(const FrameDesc& desc);
    void build_frame_passes(const FrameDesc& desc,
                            std::unordered_map<IScene*, SceneState>& consumed_scenes,
                            FrameSlot& slot);
    bool view_matches(const ViewEntry& entry, const FrameDesc& desc) const;
    FrameContext make_frame_context();

    IRenderBackend::Ptr backend_;
    IRenderContext* render_ctx_ = nullptr;

    // resources_ must outlive any member that holds IProgram::Ptr
    // refs (views_, batch_builder_): material dtors invoke
    // on_gpu_resource_destroyed which calls into resources_.
    IGpuResourceManager::Ptr resources_;

    vector<ViewEntry> views_;
    const std::unordered_map<uint64_t, PipelineId>* pipeline_map_ = nullptr;

    struct DebugOverlay {
        IWindowSurface::Ptr surface;
        TextureId texture_id = 0;
        rect dst_rect{};
    };
    vector<DebugOverlay> debug_overlays_;

    // One-shot flag set by request_bvh_log(), consumed in the next
    // BVH-emit cb. The cb walks every mesh instance and prints
    // (instance_index, buffer_addr, ibo_offset, triangle_count) so the
    // log can be compared against the F12-dumped shadow_debug image.
    bool log_bvh_next_ = false;

    // Shared visual-command / gpu-resource cache. Both the dirty-resource
    // upload in consume_scenes and the raster paths read from it, so it
    // lives here rather than inside a single sub-renderer.
    BatchBuilder batch_builder_;

    // Frame data buffer (per-slot GPU staging). Slot lifecycle is on the
    // interface, so a single Ptr is sufficient.
    IFrameDataManager::Ptr frame_buffer_;

    FrameSlot* active_slot_ = nullptr;

    // Shared per-frame scene snippet registry (materials + shadow
    // techniques). Owned here so both the scene-wide BVH build and
    // the sub-renderers (RayTracer composer, DeferredLighter light
    // resolution) read from the same stable ids.
    IFrameSnippetRegistry::Ptr snippets_;

    // Per-scene concrete SceneBvh pointer cache. The attachment on the
    // scene root owns the lifetime; we just cache the typed pointer so
    // rebuild() can be called without a round-trip through IBvh. Stale
    // entries for dead scenes are harmless because we only touch entries
    // whose scene is currently in `consumed_scenes`.
    std::unordered_map<IScene*, impl::SceneBvh*> scene_bvh_cache_;

    // Shared RTT (render-to-texture) subtree cache. Used by Forward +
    // Deferred raster paths via FrameContext.
    RenderTargetCache render_target_cache_;

    // Built-in fallback. Used when a camera has no IRenderPath attached
    // so trivial UI samples don't have to opt in. Created via the type
    // registry alongside the rest of the per-renderer plumbing.
    IRenderPath::Ptr default_forward_path_;

    // Set of paths the Renderer has dispatched to during this run.
    // Populated by build_frame_passes; iterated for lifecycle hooks
    // (on_view_removed, on_element_removed, shutdown, build_shared_passes)
    // so we don't have to track which path was last used by which view.
    std::unordered_set<IRenderPath*> seen_paths_;

    static constexpr uint64_t kGpuLatencyFrames = 3;
    static constexpr uint32_t kDefaultMaxFramesInFlight = kGpuLatencyFrames + 1;
    vector<FrameSlot> frame_slots_{kDefaultMaxFramesInFlight};
    uint64_t next_frame_id_ = 1;
    uint64_t present_counter_ = 0;
    std::mutex slot_mutex_;
    std::condition_variable slot_cv_;
};

} // namespace velk

#endif // VELK_UI_RENDERER_IMPL_H
