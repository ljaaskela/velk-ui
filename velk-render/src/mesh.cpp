#include "mesh.h"

#include <velk/api/velk.h>

namespace velk::impl {

void MeshPrimitive::init(const IMeshBuffer::Ptr& buffer,
                          uint32_t vertex_offset, uint32_t vertex_count,
                          uint32_t index_offset, uint32_t index_count,
                          array_view<VertexAttribute> attributes,
                          uint32_t vertex_stride,
                          MeshTopology topology,
                          const aabb& bounds,
                          const IMeshBuffer::Ptr& uv1_buffer,
                          uint32_t uv1_offset)
{
    buffer_ = buffer;
    vertex_offset_ = vertex_offset;
    vertex_count_ = vertex_count;
    index_offset_ = index_offset;
    index_count_ = index_count;
    attributes_.assign(attributes.begin(), attributes.end());
    vertex_stride_ = vertex_stride;
    topology_ = topology;
    bounds_ = bounds;
    uv1_buffer_ = uv1_buffer;
    uv1_offset_ = uv1_offset;
}

void Mesh::init(array_view<IMeshPrimitive::Ptr> primitives,
                const aabb& bounds, bool has_explicit_bounds)
{
    primitives_.assign(primitives.begin(), primitives.end());
    if (has_explicit_bounds) {
        bounds_ = bounds;
        bounds_known_ = true;
    }
}

aabb Mesh::get_bounds() const
{
    std::call_once(bounds_once_, [&]() {
        if (bounds_known_) return;
        if (primitives_.empty()) {
            bounds_ = aabb{};
        } else {
            bounds_ = primitives_.front()->get_bounds();
            for (size_t i = 1; i < primitives_.size(); ++i) {
                bounds_ = aabb::merge(bounds_, primitives_[i]->get_bounds());
            }
        }
        bounds_known_ = true;
    });
    return bounds_;
}

} // namespace velk::impl
