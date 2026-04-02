#ifndef VELK_UI_API_MATERIAL_GRADIENT_H
#define VELK_UI_API_MATERIAL_GRADIENT_H

#include <velk/api/object.h>
#include <velk/api/state.h>

#include <velk-ui/interface/intf_gradient.h>
#include <velk-ui/interface/intf_material.h>
#include <velk-ui/plugin.h>

namespace velk_ui {

/**
 * @brief Convenience wrapper around a GradientMaterial.
 *
 * Provides null-safe access to gradient properties (start_color, end_color, angle).
 *
 *   auto grad = material::create_gradient();
 *   grad.set_start_color(velk::color::red());
 *   grad.set_end_color(velk::color::blue());
 *   grad.set_angle(90.f);
 *   rect.set_paint(grad);
 */
class GradientMaterial : public velk::Object
{
public:
    GradientMaterial() = default;
    explicit GradientMaterial(velk::IObject::Ptr obj) : Object(check_object<IGradient>(obj)) {}
    operator IMaterial::Ptr() const { return as_ptr<IMaterial>(); }

    void set_gradient(velk::color start_color, velk::color end_color, float angle)
    {
        if (auto wh = write_state<IGradient>()) {
            wh->start_color = start_color;
            wh->end_color = end_color;
            wh->angle = angle;
        }
    }

    auto get_start_color() const { return read_state_value<IGradient>(&IGradient::State::start_color); }
    void set_start_color(const velk::color& v) { write_state_value<IGradient>(&IGradient::State::start_color, v); }

    auto get_end_color() const { return read_state_value<IGradient>(&IGradient::State::end_color); }
    void set_end_color(const velk::color& v) { write_state_value<IGradient>(&IGradient::State::end_color, v); }

    auto get_angle() const { return read_state_value<IGradient>(&IGradient::State::angle); }
    void set_angle(float v) { write_state_value<IGradient>(&IGradient::State::angle, v); }
};

namespace material {

/** @brief Creates a new GradientMaterial. */
inline GradientMaterial create_gradient(velk::color start_color = velk::color::white(),
                                        velk::color end_color = velk::color::black(), float angle = 0.f)
{
    auto grad = GradientMaterial(velk::instance().create<velk::IObject>(ClassId::Material::Gradient));
    grad.set_gradient(start_color, end_color, angle);
    return grad;
}

} // namespace material

} // namespace velk_ui

#endif // VELK_UI_API_MATERIAL_GRADIENT_H
