#ifndef VELK_UI_INTF_TRS_H
#define VELK_UI_INTF_TRS_H

#include <velk/api/math_types.h>
#include <velk/interface/intf_metadata.h>

namespace velk_ui {

/**
 * @brief Decomposed transform: translate, rotate (Z), scale.
 *
 * Applied as T * R * S after layout is finalized.
 * Rotation is in degrees around the Z axis.
 */
class ITrs : public velk::Interface<ITrs>
{
public:
    VELK_INTERFACE(
        (PROP, velk::vec3, translate, {}),
        (PROP, float, rotation, 0.f),
        (PROP, velk::vec2, scale, (velk::vec2{1.f, 1.f}))
    )
};

} // namespace velk_ui

#endif // VELK_UI_INTF_TRS_H
