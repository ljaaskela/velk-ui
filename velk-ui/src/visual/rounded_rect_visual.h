#ifndef VELK_UI_ROUNDED_RECT_VISUAL_H
#define VELK_UI_ROUNDED_RECT_VISUAL_H

#include <velk-render/interface/intf_analytic_shape.h>
#include <velk-render/interface/intf_shader_source.h>
#include <velk-scene/ext/trait.h>
#include <velk-ui/plugin.h>

namespace velk::ui {

/**
 * @brief Rounded rectangle visual.
 *
 * Produces a single FillRoundedRect draw command. The renderer uses
 * an SDF fragment shader to clip corners with antialiasing.
 */
class RoundedRectVisual : public ::velk::ext::Visual2D<RoundedRectVisual,
                                                ::velk::IAnalyticShape>
{
public:
    VELK_CLASS_UID(ClassId::Visual::RoundedRect, "RoundedRectVisual");

    vector<DrawEntry> get_draw_entries(::velk::IRenderContext& ctx,
                                       const ::velk::size& bounds) override;

    // IShaderSource: custom fragment for SDF corners (vertex stays
    // default), plus a `velk_visual_discard()` body so the deferred
    // gbuffer composer clips rounded corners in the deferred pass too,
    // plus an `intersect` snippet for the RT path.
    ::velk::string_view get_source(::velk::string_view role) const override;
    ::velk::string_view get_fn_name(::velk::string_view role) const override;
    uint64_t get_pipeline_key() const override;

    // IAnalyticShape: reports shape_kind 0 (a rect); the registered
    // intersect snippet (under shader_role::kIntersect) enables corner
    // radius on emitted shadow / RT shapes.
    uint32_t get_shape_kind() const override { return 0; }
};

} // namespace velk::ui

#endif // VELK_UI_ROUNDED_RECT_VISUAL_H
