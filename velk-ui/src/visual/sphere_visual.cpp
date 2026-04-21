#include "sphere_visual.h"

#include "primitive_shaders.h"

#include <velk/api/state.h>
#include <velk-render/gpu_data.h>
#include <velk-render/interface/intf_mesh.h>

#include <cstring>

namespace velk::ui {

namespace {

VELK_GPU_STRUCT SphereInstanceCpu
{
    ::velk::mat4 world_matrix;
    float size[4];
    float color[4];
};

} // namespace

vector<DrawEntry> SphereVisual::get_draw_entries(const ::velk::size& bounds)
{
    auto vs = read_state<IVisual>(this);
    ::velk::color col = vs ? vs->color : ::velk::color::white();

    DrawEntry entry{};
    entry.pipeline_key = kPrimitive3DPipelineKey;

    SphereInstanceCpu inst{};
    inst.size[0] = bounds.width;
    inst.size[1] = bounds.height;
    inst.size[2] = bounds.depth;
    inst.size[3] = 0.f;
    inst.color[0] = col.r;
    inst.color[1] = col.g;
    inst.color[2] = col.b;
    inst.color[3] = col.a;
    entry.set_instance(inst);

    return { entry };
}

::velk::IMesh::Ptr SphereVisual::get_mesh(::velk::IRenderContext& ctx) const
{
    auto ps = read_state<::velk::IPrimitiveShape>(this);
    uint32_t subs = ps ? ps->subdivisions : 0;
    return ctx.get_mesh_builder().get_sphere(subs);
}

::velk::ShaderSource SphereVisual::get_raster_source(::velk::IRasterShader::Target t) const
{
    if (t == ::velk::IRasterShader::Target::Forward) {
        return { primitive3d_vertex_src, primitive3d_fragment_src };
    }
    return {};
}

uint64_t SphereVisual::get_raster_pipeline_key() const
{
    return kPrimitive3DPipelineKey;
}

} // namespace velk::ui
