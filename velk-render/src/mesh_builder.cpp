#include "mesh_builder.h"

#include "mesh.h"

#include <velk/api/velk.h>

namespace velk::impl {

IMesh::Ptr MeshBuilder::build(array_view<VertexAttribute> attributes,
                              uint32_t vertex_stride,
                              const void* vertex_data, uint32_t vertex_count,
                              const uint32_t* indices, uint32_t index_count,
                              MeshTopology topology,
                              const aabb& bounds)
{
    auto mesh_intf = ::velk::instance().create<IMesh>(::velk::ClassId::Mesh);
    auto* mesh = dynamic_cast<Mesh*>(mesh_intf.get());
    if (!mesh) return nullptr;

    mesh->init(attributes, vertex_stride,
               vertex_data, vertex_count,
               indices, index_count,
               topology, bounds);
    return mesh_intf;
}

IMesh::Ptr MeshBuilder::get_unit_quad()
{
    if (unit_quad_) {
        return unit_quad_;
    }

    // 4 vec2 vertices arranged for TriangleStrip:
    // strip order (0,1,2)(1,2,3) makes triangles (0,0)-(1,0)-(0,1) and
    // (1,0)-(0,1)-(1,1). No IBO; drawn with vkCmdDraw(vertex_count=4).
    // VS runs exactly once per unique vertex.
    static const float verts[] = {
        0.f, 0.f,
        1.f, 0.f,
        0.f, 1.f,
        1.f, 1.f,
    };
    static const VertexAttribute attrs[] = {
        { VertexAttributeSemantic::Position, VertexAttributeFormat::Float2, 0 },
    };

    aabb bounds{};
    bounds.position = { 0.f, 0.f, 0.f };
    bounds.extent = { 1.f, 1.f, 0.f };

    unit_quad_ = build({ attrs, 1 },
                       /*vertex_stride*/ sizeof(float) * 2,
                       verts, /*vertex_count*/ 4,
                       /*indices*/ nullptr, /*index_count*/ 0,
                       MeshTopology::TriangleStrip,
                       bounds);
    return unit_quad_;
}

} // namespace velk::impl
