#ifndef VELK_RENDER_INTF_ANALYTIC_SHAPE_H
#define VELK_RENDER_INTF_ANALYTIC_SHAPE_H

#include <velk/interface/intf_metadata.h>
#include <velk/string_view.h>

#include <cstdint>

namespace velk {

class IRenderContext;


/**
 * @brief Contract for analytic primitive shapes consumed by the RT
 *        path's shape-kind dispatch.
 *
 * Built-in primitives (rect / cube / sphere) are enumerated by
 * `get_shape_kind()` and dispatched in the RT compute prelude.
 *
 * Visuals that ship a one-off intersect (rounded box, capsule, SDF
 * blob) implement `IShaderSource` alongside `IAnalyticShape` and
 * supply the snippet under role `shader_role::kIntersect`. The RT
 * composer queries the registry for active intersect snippets and
 * splices each into the shape-kind dispatch switch.
 */
class IAnalyticShape : public Interface<IAnalyticShape>
{
public:
    /**
     * @brief Identifies the primitive kind for the RT shape buffer.
     *
     * Built-in kinds reserved by the RT prelude:
     *   0 = rect (planar quad)
     *   1 = cube (oriented box)
     *   2 = sphere
     *
     * Visuals that ship a custom intersect snippet return one of
     * these (usually 0 for rect-based variants like a rounded
     * rectangle or a text glyph quad) so the emitted shape data fits
     * the same `RtShape` layout. The renderer's composer replaces the
     * built-in dispatch with one that also calls the custom intersect
     * for shapes whose emitting visual registered a snippet.
     */
    virtual uint32_t get_shape_kind() const = 0;
};

} // namespace velk

#endif // VELK_RENDER_INTF_ANALYTIC_SHAPE_H
