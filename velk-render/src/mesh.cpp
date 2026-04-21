#include "mesh.h"

#include <velk/api/velk.h>

namespace velk::impl {

Mesh::Mesh() = default;

void Mesh::init(array_view<VertexAttribute> attributes,
                uint32_t vertex_stride,
                const void* vertex_data, uint32_t vertex_count,
                const uint32_t* indices, uint32_t index_count,
                MeshTopology topology,
                const aabb& bounds)
{
    attributes_.assign(attributes.begin(), attributes.end());
    vertex_stride_ = vertex_stride;
    vertex_count_ = vertex_count;
    index_count_ = index_count;
    topology_ = topology;
    bounds_ = bounds;

    if (!buffer_) {
        buffer_ = ::velk::instance().create<IMeshBuffer>(::velk::ClassId::MeshBuffer);
    }
    if (buffer_) {
        const size_t vbo_size = size_t(vertex_count) * vertex_stride;
        const size_t ibo_size = indices ? size_t(index_count) * sizeof(uint32_t) : 0;
        buffer_->set_data(vertex_data, vbo_size, indices, ibo_size);
    }
}

} // namespace velk::impl
