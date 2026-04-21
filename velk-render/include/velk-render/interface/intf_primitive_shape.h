#ifndef VELK_RENDER_INTF_PRIMITIVE_SHAPE_H
#define VELK_RENDER_INTF_PRIMITIVE_SHAPE_H

#include <velk/interface/intf_metadata.h>

#include <cstdint>

namespace velk {

/**
 * @brief Side-interface declaring "this is a procedurally generated
 *        primitive shape" (cube, sphere, plane, cylinder, ...).
 *
 * Sits alongside IAnalyticShape as a velk-render-level role. Does not
 * carry geometry itself; a visual that is both IPrimitiveShape and
 * IMeshVisual (velk-ui) derives its mesh from the renderer's mesh
 * builder, cached by subdivisions.
 *
 * `subdivisions` is a single density knob. 0 means "shape-specific
 * default" (e.g. 1 quad per face for cube, 16 segments for sphere).
 * Higher values = more tessellation. Each shape documents its own
 * interpretation.
 */
class IPrimitiveShape : public Interface<IPrimitiveShape>
{
public:
    VELK_INTERFACE(
        (PROP, uint32_t, subdivisions, 0)
    )
};

} // namespace velk

#endif // VELK_RENDER_INTF_PRIMITIVE_SHAPE_H
