#include "resource/program_data_buffer.h"

#include <cstring>

namespace velk::impl {

bool ProgramDataBuffer::write(size_t sz, WriteFn fn, void* ctx)
{
    if (sz == 0 || !fn) {
        if (!bytes_.empty()) {
            bytes_.clear();
            dirty_ = true;
            return true;
        }
        return false;
    }
    if (pending_.size() != sz) {
        pending_.resize(sz, 0);
    }
    std::memset(pending_.data(), 0, sz);
    fn(pending_.data(), sz, ctx);
    if (bytes_.size() != sz
        || std::memcmp(bytes_.data(), pending_.data(), sz) != 0) {
        std::swap(bytes_, pending_);
        dirty_ = true;
        return true;
    }
    return false;
}

} // namespace velk::impl
