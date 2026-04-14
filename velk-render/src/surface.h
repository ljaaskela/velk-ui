#ifndef VELK_RENDER_SURFACE_IMPL_H
#define VELK_RENDER_SURFACE_IMPL_H

#include <velk-render/ext/gpu_resource.h>
#include <velk-render/interface/intf_surface.h>
#include <velk-render/plugin.h>

namespace velk {

class Surface : public ext::GpuResource<Surface, ISurface>
{
public:
    VELK_CLASS_UID(ClassId::Surface, "Surface");

    GpuResourceType get_type() const override { return GpuResourceType::Surface; }
    uint64_t get_render_target_id() const override { return render_target_id_; }
    void set_render_target_id(uint64_t id) { render_target_id_ = id; }

private:
    uint64_t render_target_id_ = 0;
};

} // namespace velk

#endif // VELK_RENDER_SURFACE_IMPL_H
