#include <velk-render/debug/render_target_dump.h>

#include <velk/string.h>
#include <velk/vector.h>

namespace velk::render::debug {

bool dump_texture(IRenderBackend& backend, TextureId texture,
                  const IImageWriter& writer, string_view path_no_ext)
{
    vector<uint8_t> pixels;
    PixelFormat format{};
    uvec2 dims{};
    if (!backend.read_texture(texture, pixels, format, dims)) {
        return false;
    }

    string path(path_no_ext);
    const int w = static_cast<int>(dims.x);
    const int h = static_cast<int>(dims.y);

    switch (format) {
    case PixelFormat::RGBA8:
    case PixelFormat::RGBA8_SRGB:
    case PixelFormat::R8:
        path += ".png";
        return writer.save_png(w, h, format, pixels.data(), pixels.size(), path);
    case PixelFormat::RGBA32F:
        path += ".hdr";
        return writer.save_hdr(w, h, format, pixels.data(), pixels.size(), path);
    case PixelFormat::RGBA16F:
    default:
        return false;
    }
}

} // namespace velk::render::debug
