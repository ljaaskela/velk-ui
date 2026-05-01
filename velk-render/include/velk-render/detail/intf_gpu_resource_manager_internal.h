#ifndef VELK_RENDER_INTF_GPU_RESOURCE_MANAGER_INTERNAL_H
#define VELK_RENDER_INTF_GPU_RESOURCE_MANAGER_INTERNAL_H

#include <velk/interface/intf_metadata.h>

#include <velk-render/interface/intf_gpu_resource.h>
#include <velk-render/interface/intf_gpu_resource_manager.h>
#include <velk-render/interface/intf_render_backend.h>
#include <velk-render/render_types.h>

namespace velk {

/**
 * @brief Framework-internal lifecycle API for the GPU resource manager.
 *
 * Pipeline-side code (paths, view pipelines, post-process effects)
 * sees only `IGpuResourceManager` (the factory + lookup surface) and
 * uses managed `IGpuResource::Ptr`s whose dtors auto-defer through this
 * internal interface via the observer chain. Direct callers of these
 * methods are framework code only:
 *  - `Renderer` itself (drain, shutdown, observer forwarding,
 *    set_lifecycle at startup).
 *  - `FrameSnippetRegistry` (buffer realloc on size change).
 *  - `RenderTargetCache` (RTT realloc on resize / format change —
 *    pending migration to a managed Ptr too).
 */
class IGpuResourceManagerInternal
    : public Interface<IGpuResourceManagerInternal, IGpuResourceManager>
{
public:
    /// Set by the renderer at startup. The manager uses these to create
    /// backend resources (factory methods on IGpuResourceManager) and
    /// to subscribe the renderer's observer so dropping the last Ptr
    /// to a managed resource auto-defers its backend handle.
    virtual void set_lifecycle(IRenderBackend* backend,
                               IGpuResourceObserver* observer) = 0;

    /// Deferred destruction. The handle is enqueued and only destroyed
    /// once GPU work tagged with `completion_marker` has finished
    /// (queried via `IRenderBackend::is_frame_complete`). The right
    /// marker for resources still referenced by the in-flight frame is
    /// `IRenderBackend::pending_frame_completion_marker()`.
    virtual void defer_texture_destroy(TextureId tid, uint64_t completion_marker) = 0;
    virtual void defer_buffer_destroy(GpuBuffer handle, uint64_t completion_marker) = 0;
    virtual void defer_pipeline_destroy(PipelineId pid, uint64_t completion_marker) = 0;

    /// Drains entries whose completion marker has resolved.
    virtual void drain_deferred(IRenderBackend& backend) = 0;

    /// Hook called when a tracked GPU resource is destroyed CPU-side.
    /// Enqueues backend handles for deferred destruction tagged with
    /// @p completion_marker.
    virtual void on_resource_destroyed(IGpuResource* resource,
                                       uint64_t completion_marker) = 0;

    /// Destroys all tracked resources during renderer shutdown.
    virtual void shutdown(IRenderBackend& backend) = 0;
};

/// One-liner helpers that interface_cast a public-facing
/// `IGpuResourceManager*` to its internal sibling and forward the call.
/// Callers (Renderer, FrameSnippetRegistry, RenderTargetCache) use these
/// instead of holding an `IGpuResourceManagerInternal*` separately.
/// No-op when the cast fails (defensive; should not happen since the
/// concrete manager implements both interfaces).
inline void set_lifecycle(IGpuResourceManager* mgr,
                          IRenderBackend* backend,
                          IGpuResourceObserver* observer)
{
    if (auto* in = interface_cast<IGpuResourceManagerInternal>(mgr)) {
        in->set_lifecycle(backend, observer);
    }
}

inline void defer_texture_destroy(IGpuResourceManager* mgr,
                                  TextureId tid, uint64_t marker)
{
    if (auto* in = interface_cast<IGpuResourceManagerInternal>(mgr)) {
        in->defer_texture_destroy(tid, marker);
    }
}

inline void defer_buffer_destroy(IGpuResourceManager* mgr,
                                 GpuBuffer handle, uint64_t marker)
{
    if (auto* in = interface_cast<IGpuResourceManagerInternal>(mgr)) {
        in->defer_buffer_destroy(handle, marker);
    }
}

inline void defer_pipeline_destroy(IGpuResourceManager* mgr,
                                   PipelineId pid, uint64_t marker)
{
    if (auto* in = interface_cast<IGpuResourceManagerInternal>(mgr)) {
        in->defer_pipeline_destroy(pid, marker);
    }
}

inline void drain_deferred(IGpuResourceManager* mgr, IRenderBackend& backend)
{
    if (auto* in = interface_cast<IGpuResourceManagerInternal>(mgr)) {
        in->drain_deferred(backend);
    }
}

inline void on_resource_destroyed(IGpuResourceManager* mgr,
                                  IGpuResource* resource, uint64_t marker)
{
    if (auto* in = interface_cast<IGpuResourceManagerInternal>(mgr)) {
        in->on_resource_destroyed(resource, marker);
    }
}

inline void shutdown(IGpuResourceManager* mgr, IRenderBackend& backend)
{
    if (auto* in = interface_cast<IGpuResourceManagerInternal>(mgr)) {
        in->shutdown(backend);
    }
}

} // namespace velk

#endif // VELK_RENDER_INTF_GPU_RESOURCE_MANAGER_INTERNAL_H
