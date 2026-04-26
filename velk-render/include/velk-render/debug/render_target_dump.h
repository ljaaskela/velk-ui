#ifndef VELK_RENDER_DEBUG_RENDER_TARGET_DUMP_H
#define VELK_RENDER_DEBUG_RENDER_TARGET_DUMP_H

#include <velk/string_view.h>

#include <velk-render/interface/intf_image.h>
#include <velk-render/interface/intf_render_backend.h>

namespace velk::render::debug {

/**
 * @brief Reads a texture back from the GPU and writes it to disk via @p writer.
 *
 * Picks the file format from the texture's pixel format: 8-bit integer
 * formats go through `IImageWriter::save_png`, RGBA32F goes through
 * `save_hdr`. The chosen extension is appended to @p path_no_ext, so
 * callers don't have to know the format up front.
 *
 * Synchronous — calls `IRenderBackend::read_texture`, which drains the
 * GPU queue. Intended for debug / golden-image dumps; do not call in
 * a hot frame.
 *
 * @return true if both the readback and the file write succeeded.
 */
bool dump_texture(IRenderBackend& backend, TextureId texture,
                  const IImageWriter& writer, string_view path_no_ext);

} // namespace velk::render::debug

#endif // VELK_RENDER_DEBUG_RENDER_TARGET_DUMP_H
