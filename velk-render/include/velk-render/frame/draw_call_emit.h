#ifndef VELK_RENDER_FRAME_DRAW_CALL_EMIT_H
#define VELK_RENDER_FRAME_DRAW_CALL_EMIT_H

#include <velk/api/perf.h>

#include <unordered_map>

#include <velk-render/interface/intf_frame_data_manager.h>
#include <velk-render/interface/intf_draw_data.h>
#include <velk-render/interface/intf_program.h>
#include <velk-render/interface/intf_texture_resolver.h>

#include <cstdlib>
#include <cstring>

namespace velk {

/**
 * @brief Per-frame cache used to dedupe material upload work across
 *        batches that share an IProgram.
 *
 * `IRenderContext::build_draw_calls` (and any future draw-call emitters)
 * write a material's draw-data to the frame buffer once per unique
 * program per frame and reuse the cached GPU address for subsequent
 * batches. Pass a fresh (or `clear()`-ed) cache each frame so addresses
 * don't leak across frames.
 */
struct MaterialAddrCache
{
    std::unordered_map<IProgram*, uint64_t> addrs;
    void clear() { addrs.clear(); }
};

namespace detail {

/// Header-only helper used by both `IRenderContext::build_draw_calls`
/// (defined in render_context.cpp) and the still-in-velk-scene
/// `BatchBuilder::build_gbuffer_draw_calls`. Writes a material's
/// draw-data to the frame buffer once per unique IProgram, returns
/// the cached GPU address on subsequent calls.
inline uint64_t write_material_once(IProgram* prog,
                                    IFrameDataManager& frame_data,
                                    ITextureResolver* resolver,
                                    MaterialAddrCache& cache)
{
    if (!prog) return 0;
    auto it = cache.addrs.find(prog);
    if (it != cache.addrs.end()) return it->second;

    uint64_t addr = 0;
    if (auto* dd = interface_cast<IDrawData>(prog)) {
        size_t sz = dd->get_draw_data_size();
        if (sz > 0 && (sz % 16) != 0) {
            VELK_LOG(E,
                     "Renderer: material get_draw_data_size (%zu) is not 16-byte aligned. "
                     "Use VELK_GPU_STRUCT for your material data.",
                     sz);
        }
        if (sz > 0) {
            void* scratch = std::malloc(sz);
            if (scratch) {
                std::memset(scratch, 0, sz);
                if (dd->write_draw_data(scratch, sz, resolver) == ReturnValue::Success) {
                    addr = frame_data.write(scratch, sz);
                }
                std::free(scratch);
            }
        }
    }
    cache.addrs[prog] = addr;
    return addr;
}

} // namespace detail
} // namespace velk

#endif // VELK_RENDER_FRAME_DRAW_CALL_EMIT_H
