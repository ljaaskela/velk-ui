#ifndef VELK_UI_GPU_RESOURCE_MANAGER_H
#define VELK_UI_GPU_RESOURCE_MANAGER_H

#include <velk/ext/core_object.h>
#include <velk/vector.h>

#include <unordered_map>
#include <mutex>

#include <velk-render/detail/intf_gpu_resource_manager_internal.h>
#include <velk-render/interface/intf_gpu_resource_manager.h>
#include <velk-render/interface/intf_render_target.h>
#include <velk-render/interface/intf_surface.h>
#include <velk-render/plugin.h>

namespace velk {

/**
 * @brief Concrete IGpuResourceManager backed by std::unordered_map plus
 *        per-resource deferred-destroy queues protected by a mutex.
 *
 * Owned by Renderer as `IGpuResourceManager::Ptr`; instantiated through
 * the velk type registry so the allocation participates in the hive.
 */
class GpuResourceManager
    : public ext::ObjectCore<GpuResourceManager, IGpuResourceManagerInternal>
{
public:
    VELK_CLASS_UID(ClassId::GpuResourceManager, "GpuResourceManager");

    // ITextureResolver
    TextureId resolve(ISurface* surf) const override
    {
        if (!surf) return 0;
        TextureId tid = find_texture(surf);
        if (tid == 0) {
            uint64_t rt_id = ::velk::get_render_target_id(surf);
            if (rt_id != 0) tid = static_cast<TextureId>(rt_id);
        }
        return tid;
    }

    // IGpuResourceManager
    void set_lifecycle(IRenderBackend* backend, IGpuResourceObserver* observer) override;
    IRenderTarget::Ptr create_render_texture(const TextureDesc& desc) override;
    IRenderTextureGroup::Ptr create_render_texture_group(
        const TextureGroupDesc& desc) override;

    TextureId find_texture(ISurface* surf) const override;
    void register_texture(ISurface* surf, TextureId tid) override;

    BufferEntry* find_buffer(IBuffer* buf) override;
    void register_buffer(IBuffer* buf, const BufferEntry& entry) override;
    void unregister_buffer(IBuffer* buf) override;

    bool register_pipeline(IProgram* prog, PipelineId pid) override;

    void add_env_observer(const IBuffer::WeakPtr& res) override;
    void unregister_env_observers(IGpuResourceObserver* observer) override;

    void defer_texture_destroy(TextureId tid, uint64_t completion_marker) override;
    void defer_buffer_destroy(GpuBuffer handle, uint64_t completion_marker) override;
    void defer_pipeline_destroy(PipelineId pid, uint64_t completion_marker) override;

    void drain_deferred(IRenderBackend& backend) override;

    void on_resource_destroyed(IGpuResource* resource,
                               uint64_t completion_marker) override;

    void shutdown(IRenderBackend& backend) override;

private:
    struct DeferredTextureDestroy
    {
        TextureId tid;
        uint64_t completion_marker;
    };
    struct DeferredBufferDestroy
    {
        GpuBuffer handle;
        uint64_t completion_marker;
    };
    struct DeferredPipelineDestroy
    {
        PipelineId pid;
        uint64_t completion_marker;
    };
    struct DeferredGroupDestroy
    {
        RenderTargetGroup handle;
        uint64_t completion_marker;
    };

    IRenderBackend* backend_ = nullptr;
    IGpuResourceObserver* observer_ = nullptr;

    std::unordered_map<ISurface*, TextureId> texture_map_;
    std::unordered_map<ISurface*, RenderTargetGroup> group_map_;
    std::unordered_map<IBuffer*, BufferEntry> buffer_map_;
    std::unordered_map<IProgram*, PipelineId> pipeline_map_;
    vector<DeferredTextureDestroy> deferred_textures_;
    vector<DeferredGroupDestroy> deferred_groups_;
    vector<DeferredBufferDestroy> deferred_buffers_;
    vector<DeferredPipelineDestroy> deferred_pipelines_;
    std::mutex deferred_mutex_;
    vector<IBuffer::WeakPtr> observed_env_resources_;
};

} // namespace velk

#endif // VELK_UI_GPU_RESOURCE_MANAGER_H
