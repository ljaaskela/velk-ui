#include "mesh/mesh_buffer.h"

#include <cstring>

namespace velk::impl {

void MeshBuffer::set_data(const void* vbo_data, size_t vbo_size,
                          const void* ibo_data, size_t ibo_size)
{
    bytes_.resize(vbo_size + ibo_size);
    if (vbo_size > 0 && vbo_data) {
        std::memcpy(bytes_.data(), vbo_data, vbo_size);
    }
    if (ibo_size > 0 && ibo_data) {
        std::memcpy(bytes_.data() + vbo_size, ibo_data, ibo_size);
    }
    vbo_size_ = vbo_size;
    ibo_size_ = ibo_size;
    dirty_ = true;
}

} // namespace velk::impl
