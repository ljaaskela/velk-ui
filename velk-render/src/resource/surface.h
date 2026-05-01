#ifndef VELK_RENDER_WINDOW_SURFACE_IMPL_H
#define VELK_RENDER_WINDOW_SURFACE_IMPL_H

#include <velk/api/state.h>
#include <velk-render/ext/gpu_resource.h>
#include <velk-render/interface/intf_window_surface.h>
#include <velk-render/plugin.h>

namespace velk {

class WindowSurface : public ext::GpuResource<WindowSurface, IWindowSurface>
{
public:
    VELK_CLASS_UID(ClassId::WindowSurface, "WindowSurface");

    GpuResourceType get_type() const override { return GpuResourceType::Surface; }

    uvec2 get_dimensions() const override
    {
        auto s = read_state<IWindowSurface>(this);
        return s ? s->size : uvec2{};
    }

    PixelFormat format() const override { return format_; }
    void set_format(PixelFormat fmt) override { format_ = fmt; }

    DepthFormat get_depth_format() const override { return depth_format_; }
    void set_depth_format(DepthFormat df) override { depth_format_ = df; }

    // Window surfaces get their size from system events; no-op here.
    void set_size(uint32_t /*w*/, uint32_t /*h*/) override {}

private:
    PixelFormat format_ = PixelFormat::RGBA8;
    DepthFormat depth_format_ = DepthFormat::None;
};

} // namespace velk

#endif // VELK_RENDER_WINDOW_SURFACE_IMPL_H
