#ifndef VELK_RENDER_INTF_GPU_RESOURCE_MANAGER_H
#define VELK_RENDER_INTF_GPU_RESOURCE_MANAGER_H

#include <velk/interface/intf_interface.h>
#include <velk/uid.h>

#include <velk-render/interface/intf_buffer.h>
#include <velk-render/interface/intf_gpu_resource.h>
#include <velk-render/interface/intf_program.h>
#include <velk-render/interface/intf_render_backend.h>
#include <velk-render/interface/intf_texture_resolver.h>
#include <velk-render/plugin.h>
#include <velk-render/render_types.h>

namespace velk {

class ISurface;

/**
 * @brief Tracks GPU resources for a renderer instance.
 *
 * Mediates between CPU-side IBuffer / ISurface objects and the backend's
 * opaque handles (GpuBuffer, TextureId, PipelineId). Owns deferred-destroy
 * queues so resources stay live until the GPU has consumed any in-flight
 * frames that referenced them.
 *
 * Also provides ISurface->TextureId resolution (sibling base
 * `ITextureResolver`) so materials can embed bindless texture ids in
 * their UBOs via IDrawData::write_draw_data without coupling to the
 * concrete manager.
 */
class IGpuResourceManager
    : public Interface<IGpuResourceManager, IInterface,
                       VELK_UID("04f21ebb-d725-457b-97de-3a8d81262b9f")>,
      public ITextureResolver
{
public:
    /** @brief Backend handle + size pair tracked per CPU IBuffer. */
    struct BufferEntry
    {
        GpuBuffer handle{};
        size_t size = 0;
    };

    // Texture mapping
    virtual TextureId find_texture(ISurface* surf) const = 0;
    virtual void register_texture(ISurface* surf, TextureId tid) = 0;

    // Buffer mapping
    virtual BufferEntry* find_buffer(IBuffer* buf) = 0;
    virtual void register_buffer(IBuffer* buf, const BufferEntry& entry) = 0;
    virtual void unregister_buffer(IBuffer* buf) = 0;

    /// Idempotent. Returns true if the program was newly registered (the
    /// caller should subscribe as observer in that case), false if it was
    /// already tracked.
    virtual bool register_pipeline(IProgram* prog, PipelineId pid) = 0;

    // Environment resource bookkeeping (textures referenced by env material;
    // not tracked through the element cache so they need a sidecar list).
    virtual void add_env_observer(const IBuffer::WeakPtr& res) = 0;
    virtual void unregister_env_observers(IGpuResourceObserver* observer) = 0;

    // Deferred destruction. The handle is enqueued and only destroyed
    // once present_counter has advanced past safe_after_frame.
    virtual void defer_texture_destroy(TextureId tid, uint64_t safe_after) = 0;
    virtual void defer_buffer_destroy(GpuBuffer handle, uint64_t safe_after) = 0;
    virtual void defer_pipeline_destroy(PipelineId pid, uint64_t safe_after) = 0;

    /// Drains entries whose safe-after frame has been reached.
    virtual void drain_deferred(IRenderBackend& backend, uint64_t present_counter) = 0;

    /// Hook called when a tracked GPU resource is destroyed CPU-side.
    /// Enqueues backend handles for deferred destruction.
    virtual void on_resource_destroyed(IGpuResource* resource,
                                       uint64_t present_counter,
                                       uint64_t latency_frames) = 0;

    /// Destroys all tracked resources during renderer shutdown.
    virtual void shutdown(IRenderBackend& backend) = 0;
};

} // namespace velk

#endif // VELK_RENDER_INTF_GPU_RESOURCE_MANAGER_H
