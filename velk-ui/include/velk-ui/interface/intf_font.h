#ifndef VELK_UI_INTF_FONT_H
#define VELK_UI_INTF_FONT_H

#include <velk/api/math_types.h>
#include <velk/interface/intf_metadata.h>
#include <velk/string_view.h>
#include <velk/vector.h>

#include <cstdint>

namespace velk::ui {

class IFont : public Interface<IFont>
{
public:
    struct GlyphPosition
    {
        uint32_t glyph_id;
        vec2 offset;  // pixels, relative to pen position
        vec2 advance; // pixels, how far to move the pen
    };

    struct GlyphBitmap
    {
        const uint8_t* data; // alpha bitmap, valid until next rasterize call
        uint32_t width;
        uint32_t height;
        vec2 bearing; // left and top side bearing (pixels)
    };

    /** @brief Position and metrics of a glyph in the font's atlas. */
    struct GlyphRect
    {
        uint32_t x, y, w, h;
        float bearing_x, bearing_y;
    };

    VELK_INTERFACE(
        (RPROP, float, ascender, 0.f),
        (RPROP, float, descender, 0.f),
        (RPROP, float, line_height, 0.f),
        (RPROP, float, size_px, 0.f)
    )

    /** @brief Initializes the font from a built-in default (embedded Inter Regular). */
    virtual bool init_default() = 0;

    virtual bool set_size(float size_px) = 0;
    virtual float shape_text(string_view text, vector<GlyphPosition>& out) = 0;
    virtual GlyphBitmap rasterize_glyph(uint32_t glyph_id) = 0;

    /**
     * @brief Ensures a glyph is rasterized and placed in the font's atlas.
     * @param glyph_id Glyph index from shape_text().
     * @return Pointer to the glyph's atlas rect, or nullptr on failure.
     */
    virtual const GlyphRect* ensure_glyph(uint32_t glyph_id) = 0;

    /** @brief Returns the atlas width in pixels. */
    virtual uint32_t get_atlas_width() const = 0;

    /** @brief Returns the atlas height in pixels. */
    virtual uint32_t get_atlas_height() const = 0;
};

} // namespace velk::ui

#endif // VELK_UI_INTF_FONT_H
