#ifndef VELK_RENDER_GBUFFER_H
#define VELK_RENDER_GBUFFER_H

#include <cstdint>

#include <velk-render/interface/intf_render_backend.h>

namespace velk {

/**
 * @brief Canonical G-buffer attachment layout for the deferred pipeline.
 *
 * Every raster material that participates in deferred shading writes to
 * these four color targets in order. The compute lighting pass samples
 * them to reconstruct shading.
 *
 * Keep this layout stable: all material shaders, the render pass, and
 * the lighting pass are compiled against these indices + formats.
 */
enum class GBufferAttachment : uint32_t
{
    Albedo         = 0, ///< RGBA8      rgb = surface albedo, a = alpha.
    Normal         = 1, ///< RGBA16F    xyz = world-space normal, w = unused.
    WorldPos       = 2, ///< RGBA32F    xyz = world position, w = unused. Full float because
                        ///  RGBA16F's ~3 decimal digits of precision quantizes a typical
                        ///  100 m scene to ~0.1 m steps, which collapses entire raster
                        ///  triangles into a single shader-visible position and breaks
                        ///  any per-pixel computation that needs world-space coords
                        ///  (notably RT shadows: triangle-granularity hit/miss).
    MaterialParams = 3, ///< RGBA8      r = metallic, g = roughness, b = lighting mode, a = unused.
    Count          = 4
};

/** @brief Canonical G-buffer attachment formats in declaration order. */
inline constexpr PixelFormat kGBufferFormats[static_cast<uint32_t>(GBufferAttachment::Count)] = {
    PixelFormat::RGBA8,    // Albedo
    PixelFormat::RGBA16F,  // Normal
    PixelFormat::RGBA32F,  // WorldPos (precision-critical; see comment above)
    PixelFormat::RGBA8,    // MaterialParams
};

/**
 * @brief Tells the compute lighting pass how to interpret a G-buffer pixel.
 *
 * Encoded in MaterialParams.b (scaled by 255 on write, compared by range
 * on read). Add new modes by appending before Count.
 */
enum class LightingMode : uint8_t
{
    Unlit    = 0, ///< Pass albedo through. Used by UI visuals that draw their own color (text, gradient, rect).
    Standard = 1, ///< PBR direct lighting + shadows + env. Used by StandardMaterial.
    Count
};

} // namespace velk

#endif // VELK_RENDER_GBUFFER_H
