#ifndef VELK_RENDER_INTF_RENDERER_INTERNAL_H
#define VELK_RENDER_INTF_RENDERER_INTERNAL_H

#include <velk/interface/intf_metadata.h>

#include <velk-render/interface/intf_render_backend.h>
#include <velk-render/interface/intf_render_context.h>

namespace velk {

/**
 * @brief Internal interface for injecting the backend into a renderer.
 *
 * Not exposed to the app. The RenderContext uses this to connect the
 * backend and context to the renderer implementation after creation.
 */
class IRendererInternal : public Interface<IRendererInternal>
{
public:
    virtual void set_backend(const IRenderBackend::Ptr& backend,
                             IRenderContext* ctx) = 0;

    /// Returns and clears the duration (in nanoseconds) the most
    /// recent `prepare()` spent blocked on GPU completion before it
    /// could claim a frame slot. Lets the perf overlay charge that
    /// time as "GPU wait" rather than CPU work.
    virtual uint64_t consume_last_prepare_gpu_wait_ns() = 0;
};

} // namespace velk

#endif // VELK_RENDER_INTF_RENDERER_INTERNAL_H
