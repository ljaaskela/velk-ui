#ifndef VELK_RENDER_INTF_RENDER_TARGET_H
#define VELK_RENDER_INTF_RENDER_TARGET_H

#include <velk-render/interface/intf_surface.h>

namespace velk {

/**
 * @brief A surface that can be rendered into.
 *
 * Both swapchain windows and renderable textures implement this interface.
 * The render target id is the backend handle passed to begin_pass().
 *
 * Chain: IInterface -> IGpuResource -> ISurface -> IRenderTarget
 */
class IRenderTarget : public Interface<IRenderTarget, ISurface>
{
public:
    // Bind handle (the value passed to backend's `begin_pass`) is
    // accessed via `IGpuResource::get_gpu_handle(GpuResourceKey::Default)`.
    // For window surfaces the handle is the surface_id from
    // `create_surface`; for render textures it's the TextureId.

    /**
     * @brief Returns the depth attachment format of this target.
     *
     * DepthFormat::None means the target has no depth attachment. The backend
     * uses this to decide whether to wire depth state on pipelines and to
     * clear depth at pass begin.
     */
    virtual DepthFormat get_depth_format() const = 0;

    /** @brief Sets the depth attachment format. Called during target creation. */
    virtual void set_depth_format(DepthFormat df) = 0;
};

/**
 * @brief Returns the bind handle from any interface pointer that supports IRenderTarget.
 *        Convenience over `interface_cast<IRenderTarget>(p)->get_gpu_handle(GpuResourceKey::Default)`.
 */
template <typename T>
uint64_t get_render_target_id(const T& ptr)
{
    auto* rt = interface_cast<IRenderTarget>(ptr);
    return rt ? rt->get_gpu_handle(GpuResourceKey::Default) : 0;
}

} // namespace velk

#endif // VELK_RENDER_INTF_RENDER_TARGET_H
