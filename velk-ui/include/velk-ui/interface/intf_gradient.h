#ifndef VELK_UI_INTF_GRADIENT_H
#define VELK_UI_INTF_GRADIENT_H

#include <velk/api/math_types.h>
#include <velk/interface/intf_metadata.h>

namespace velk_ui {

/**
 * @brief Interface for linear gradient properties.
 *
 * Provides start color, end color, and angle for a linear gradient.
 * Properties are mapped to shader uniforms by the renderer via metadata
 * introspection (uniform names match property names with a u_ prefix).
 */
class IGradient : public velk::Interface<IGradient>
{
public:
    VELK_INTERFACE(
        (PROP, velk::color, start_color, (velk::color::white())),
        (PROP, velk::color, end_color, (velk::color::black())),
        (PROP, float, angle, 0.f)
    )
};

} // namespace velk_ui

#endif // VELK_UI_INTF_GRADIENT_H
