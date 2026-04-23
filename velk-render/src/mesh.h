#ifndef VELK_RENDER_MESH_H
#define VELK_RENDER_MESH_H

#include <velk/ext/object.h>
#include <velk/vector.h>

#include <velk-render/interface/intf_mesh.h>
#include <velk-render/plugin.h>

#include <mutex>

namespace velk::impl {

/// Concrete IMeshPrimitive. Holds the geometry range into an
/// IMeshBuffer (may be shared with sibling primitives), the attribute
/// layout, topology, bounds, and a material ObjectRef.
class MeshPrimitive
    : public ::velk::ext::Object<MeshPrimitive, IMeshPrimitive>
{
public:
    VELK_CLASS_UID(::velk::ClassId::MeshPrimitive, "MeshPrimitive");

    MeshPrimitive() = default;

    /// Internal: populates the primitive from a pre-uploaded or
    /// pre-filled IMeshBuffer. Called by MeshBuilder; not part of the
    /// IMeshPrimitive interface so primitives stay immutable from the
    /// consumer's perspective.
    void init(const IMeshBuffer::Ptr& buffer,
              uint32_t vertex_offset, uint32_t vertex_count,
              uint32_t index_offset, uint32_t index_count,
              array_view<VertexAttribute> attributes,
              uint32_t vertex_stride,
              MeshTopology topology,
              const aabb& bounds);

    IMeshBuffer::Ptr get_buffer() const override { return buffer_; }
    uint32_t get_vertex_offset() const override { return vertex_offset_; }
    uint32_t get_vertex_count() const override { return vertex_count_; }
    uint32_t get_index_offset() const override { return index_offset_; }
    uint32_t get_index_count() const override { return index_count_; }
    array_view<VertexAttribute> get_attributes() const override
    {
        return {attributes_.data(), attributes_.size()};
    }
    uint32_t get_vertex_stride() const override { return vertex_stride_; }
    MeshTopology get_topology() const override { return topology_; }
    aabb get_bounds() const override { return bounds_; }

private:
    IMeshBuffer::Ptr buffer_;
    uint32_t vertex_offset_ = 0;
    uint32_t vertex_count_ = 0;
    uint32_t index_offset_ = 0;
    uint32_t index_count_ = 0;
    ::velk::vector<VertexAttribute> attributes_;
    uint32_t vertex_stride_ = 0;
    MeshTopology topology_ = MeshTopology::TriangleList;
    aabb bounds_{};
};

/// Concrete IMesh container. Stores a list of primitives and a lazily
/// computed aggregate bounds.
class Mesh
    : public ::velk::ext::Object<Mesh, IMesh>
{
public:
    VELK_CLASS_UID(::velk::ClassId::Mesh, "Mesh");

    Mesh() = default;

    /// Internal: installs the primitive list. Bounds is taken as-is
    /// when `has_explicit_bounds` is true (procedural shapes know their
    /// exact extent analytically); otherwise the aggregate is computed
    /// lazily on first `get_bounds` call from the primitive bounds.
    void init(array_view<IMeshPrimitive::Ptr> primitives,
              const aabb& bounds, bool has_explicit_bounds);

    array_view<IMeshPrimitive::Ptr> get_primitives() const override
    {
        return {primitives_.data(), primitives_.size()};
    }
    aabb get_bounds() const override;

private:
    ::velk::vector<IMeshPrimitive::Ptr> primitives_;
    mutable aabb bounds_{};
    mutable bool bounds_known_ = false;
    mutable std::once_flag bounds_once_;
};

} // namespace velk::impl

#endif // VELK_RENDER_MESH_H
