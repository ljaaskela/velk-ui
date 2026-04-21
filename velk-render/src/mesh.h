#ifndef VELK_RENDER_MESH_H
#define VELK_RENDER_MESH_H

#include <velk/ext/object.h>
#include <velk/vector.h>

#include <velk-render/interface/intf_mesh.h>
#include <velk-render/plugin.h>

namespace velk::impl {

/// Concrete IMesh holding one IMeshBuffer with both VBO and IBO data,
/// plus the attribute layout that describes the vertex packing.
class Mesh
    : public ::velk::ext::Object<Mesh, IMesh>
{
public:
    VELK_CLASS_UID(::velk::ClassId::Mesh, "Mesh");

    Mesh();

    /// Internal: populates the mesh's storage. Called by MeshBuilder;
    /// not part of the IMesh interface so the mesh stays immutable from
    /// the consumer's perspective.
    void init(array_view<VertexAttribute> attributes,
              uint32_t vertex_stride,
              const void* vertex_data, uint32_t vertex_count,
              const uint32_t* indices, uint32_t index_count,
              MeshTopology topology,
              const aabb& bounds);

    array_view<VertexAttribute> get_attributes() const override
    {
        return {attributes_.data(), attributes_.size()};
    }
    uint32_t get_vertex_stride() const override { return vertex_stride_; }
    uint32_t get_vertex_count() const override { return vertex_count_; }
    uint32_t get_index_count() const override { return index_count_; }
    MeshTopology get_topology() const override { return topology_; }
    aabb get_bounds() const override { return bounds_; }
    IMeshBuffer::Ptr get_buffer() const override { return buffer_; }

private:
    ::velk::vector<VertexAttribute> attributes_;
    uint32_t vertex_stride_ = 0;
    uint32_t vertex_count_ = 0;
    uint32_t index_count_ = 0;
    MeshTopology topology_ = MeshTopology::TriangleList;
    aabb bounds_{};
    IMeshBuffer::Ptr buffer_;
};

} // namespace velk::impl

#endif // VELK_RENDER_MESH_H
