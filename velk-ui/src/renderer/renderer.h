#ifndef VELK_UI_RENDERER_IMPL_H
#define VELK_UI_RENDERER_IMPL_H

#include <velk/ext/object.h>
#include <velk/vector.h>

#include <unordered_map>
#include "batch_builder.h"
#include "frame_data_manager.h"
#include "gpu_resource_manager.h"
#include <velk-render/detail/intf_renderer_internal.h>
#include <velk-render/gpu_data.h>
#include <velk-render/interface/intf_gpu_resource.h>
#include <velk-render/interface/intf_render_backend.h>
#include <velk-render/interface/intf_render_context.h>
#include <velk-render/interface/intf_render_target.h>
#include <velk-render/interface/intf_window_surface.h>
#include <velk-render/plugin.h>
#include <velk-render/render_types.h>
#include <velk-ui/interface/intf_camera.h>
#include <velk-ui/interface/intf_render_to_texture.h>
#include <velk-ui/interface/intf_renderer.h>
#include <velk-ui/interface/intf_scene.h>

#include <condition_variable>
#include <mutex>

namespace velk::ui {

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
    void shutdown() override;

private:
    struct ViewEntry
    {
        IElement::Ptr camera_element;
        IWindowSurface::Ptr surface;
        rect viewport;
        bool batches_dirty = true;
        int cached_width = 0;
        int cached_height = 0;
        vector<BatchBuilder::Batch> batches;

        // Ray-traced output texture (only allocated when camera is in RayTrace
        // path). Sized to the surface; reallocated on size change.
        TextureId rt_output_tex = 0;
        int rt_width = 0;
        int rt_height = 0;
    };

    struct RenderTarget
    {
        IRenderTarget::Ptr target;
    };

    enum class PassKind
    {
        Raster,
        ComputeBlit,
    };

    struct RenderPass
    {
        PassKind kind = PassKind::Raster;

        // Raster fields
        RenderTarget target;
        rect viewport;
        vector<DrawCall> draw_calls;

        // ComputeBlit fields: compute writes to blit_source, then blit
        // copies it into blit_dst_rect of the swapchain image for
        // blit_surface_id.
        DispatchCall compute{};
        uint64_t blit_surface_id = 0;
        TextureId blit_source = 0;
        rect blit_dst_rect{};
    };

    struct FrameSlot
    {
        uint64_t id = 0;
        vector<RenderPass> passes;
        bool ready = false;
        uint64_t presented_at = 0;
        FrameDataManager::Slot buffer;
    };

    struct RenderTargetEntry
    {
        IRenderTarget::Ptr target;
        TextureId texture_id = 0;
        int width = 0;
        int height = 0;
        bool dirty = true;
    };

    FrameSlot* claim_frame_slot();
    std::unordered_map<IScene*, SceneState> consume_scenes(const FrameDesc& desc);
    void build_frame_passes(const FrameDesc& desc,
                            std::unordered_map<IScene*, SceneState>& consumed_scenes,
                            FrameSlot& slot);
    void prepend_environment_batch(ICamera& camera, ViewEntry& entry);
    bool view_matches(const ViewEntry& entry, const FrameDesc& desc) const;

    IRenderBackend::Ptr backend_;
    IRenderContext* render_ctx_ = nullptr;

    // resources_ must outlive any member that holds IProgram::Ptr refs
    // (views_, batch_builder_): material dtors invoke on_gpu_resource_destroyed
    // which calls into resources_.
    GpuResourceManager resources_;

    vector<ViewEntry> views_;
    const std::unordered_map<uint64_t, PipelineId>* pipeline_map_ = nullptr;

    BatchBuilder batch_builder_;
    FrameDataManager frame_buffer_;

    FrameSlot* active_slot_ = nullptr;
    uint64_t globals_gpu_addr_ = 0;
    vector<DrawCall> draw_calls_;

    std::unordered_map<IElement*, RenderTargetEntry> render_target_entries_;

    // Ray-tracer material tracking. First time a painted visual's material
    // class is seen, we assign a stable small integer id (1, 2, ...; 0 is
    // reserved for "no material"). The fill snippet + function name are
    // captured so the composer can paste them into the compute shader.
    struct RtMaterialInfo
    {
        string_view fill_fn_name;
        string_view include_name;
    };
    vector<RtMaterialInfo> rt_material_info_by_id_; // index i -> material id (i+1)
    std::unordered_map<uint64_t, uint32_t> rt_material_id_by_class_;

    // Set of pipeline keys (hashes of material-id sets) already compiled.
    std::unordered_map<uint64_t, bool> rt_compiled_pipelines_;

    // Scratch: list of material ids present in the current view, sorted.
    vector<uint32_t> rt_frame_materials_;

    // Composes and compiles an RT compute pipeline for the given set of
    // material ids (ascending, unique). Returns the pipeline key, or 0 on
    // failure. Caches by a hash of the set; reused across frames.
    uint64_t ensure_rt_pipeline(const vector<uint32_t>& material_ids);

    static constexpr uint64_t kGpuLatencyFrames = 3;
    static constexpr uint32_t kDefaultMaxFramesInFlight = kGpuLatencyFrames + 1;
    vector<FrameSlot> frame_slots_{kDefaultMaxFramesInFlight};
    uint64_t next_frame_id_ = 1;
    uint64_t present_counter_ = 0;
    std::mutex slot_mutex_;
    std::condition_variable slot_cv_;
};

} // namespace velk::ui

#endif // VELK_UI_RENDERER_IMPL_H
