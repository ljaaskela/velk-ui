#ifndef VELK_RENDER_MESH_BUILDER_H
#define VELK_RENDER_MESH_BUILDER_H

#include <velk/ext/object.h>

#include <unordered_map>
#include <velk-render/interface/intf_mesh.h>
#include <velk-render/plugin.h>

namespace velk::impl {

/**
 * @brief Concrete IMeshBuilder.
 *
 * Owned by the IRenderContext (one per context). Allocates Meshes
 * through the velk type registry, populates them via the internal
 * `Mesh::init` method, and caches the engine's unit-quad singleton so
 * its lifetime is bound to the render context (released before the
 * velk runtime tears down).
 */
class MeshBuilder
    : public ::velk::ext::Object<MeshBuilder, IMeshBuilder>
{
public:
    VELK_CLASS_UID(::velk::ClassId::MeshBuilder, "MeshBuilder");

    IMesh::Ptr build(array_view<VertexAttribute> attributes,
                     uint32_t vertex_stride,
                     const void* vertex_data, uint32_t vertex_count,
                     const uint32_t* indices, uint32_t index_count,
                     MeshTopology topology,
                     const aabb& bounds) override;

    IMesh::Ptr get_unit_quad() override;
    IMesh::Ptr get_cube(uint32_t subdivisions = 0) override;
    IMesh::Ptr get_sphere(uint32_t subdivisions = 0) override;

private:
    IMesh::Ptr make_cube(uint32_t subdivisions);
    IMesh::Ptr make_sphere(uint32_t subdivisions);

    IMesh::Ptr unit_quad_;
    std::unordered_map<uint32_t, IMesh::Ptr> cube_cache_;
    std::unordered_map<uint32_t, IMesh::Ptr> sphere_cache_;
};

} // namespace velk::impl

#endif // VELK_RENDER_MESH_BUILDER_H
