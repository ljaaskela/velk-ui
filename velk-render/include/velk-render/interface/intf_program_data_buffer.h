#ifndef VELK_RENDER_INTF_PROGRAM_DATA_BUFFER_H
#define VELK_RENDER_INTF_PROGRAM_DATA_BUFFER_H

#include <velk-render/interface/intf_buffer.h>

#include <cstddef>

namespace velk {

/**
 * @brief IBuffer specialisation for persistent per-program draw data.
 *
 * Materials own an IProgramDataBuffer (typically obtained via
 * `velk::instance().create<IBuffer>(ClassId::ProgramDataBuffer)`) and
 * return it from `IDrawData::get_data_buffer()`. The buffer owns its
 * own scratch so the material can serialise through `write()` without
 * maintaining a per-instance byte vector. The buffer compares each
 * write against the previously committed bytes and only flags dirty on
 * actual change, so unchanged materials skip GPU re-uploads. The GPU
 * address stays stable across frames until the buffer's data size
 * changes (or it is replaced), which is what lets shape caches hold
 * the address across frames.
 */
class IProgramDataBuffer : public Interface<IProgramDataBuffer, IBuffer>
{
public:
    /**
     * @brief C-style writer callback. Receives the buffer's internal
     *        scratch pointer and the requested size.
     */
    using WriteFn = void (*)(void* dst, size_t sz, void* ctx);

    /**
     * @brief Asks the buffer to (re)serialise @p sz bytes via @p fn.
     *
     * The buffer supplies the destination pointer (its own scratch),
     * calls @p fn to fill it, then diffs against the previously
     * committed bytes. Returns true if the committed content changed
     * and the buffer is now dirty; false if identical (no re-upload
     * needed).
     */
    virtual bool write(size_t sz, WriteFn fn, void* ctx) = 0;

    /**
     * @brief Lambda-friendly overload that forwards to the virtual.
     *
     * Usage:
     *   pdb->write(sz, [&](void* dst, size_t n) { ... });
     */
    template <typename Writer>
    bool write(size_t sz, Writer&& writer)
    {
        auto trampoline = [](void* dst, size_t n, void* ctx) {
            (*static_cast<Writer*>(ctx))(dst, n);
        };
        return write(sz, trampoline, const_cast<Writer*>(&writer));
    }
};

} // namespace velk

#endif // VELK_RENDER_INTF_PROGRAM_DATA_BUFFER_H
