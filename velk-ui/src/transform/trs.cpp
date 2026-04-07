#include "trs.h"

#include <velk/api/state.h>
#include <velk-ui/interface/intf_element.h>

namespace velk::ui {

void Trs::transform(IElement& element)
{
    auto state = read_state<ITrs>(this);
    if (!state) {
        return;
    }

    mat4 t = mat4::translate(state->translate);
    const vec3& rot = state->rotation;
    mat4 r;
    if (rot.x == 0.f && rot.y == 0.f) {
        r = mat4::rotate_z(deg_to_rad(rot.z));
    } else {
        r = mat4::rotate_x(deg_to_rad(rot.x))
          * mat4::rotate_y(deg_to_rad(rot.y))
          * mat4::rotate_z(deg_to_rad(rot.z));
    }
    mat4 s = mat4::scale({state->scale.x, state->scale.y, 1.f});
    mat4 trs = t * r * s;

    auto elem_state = read_state<IElement>(&element);
    if (!elem_state) {
        return;
    }

    mat4 world = elem_state->world_matrix * trs;
    write_state<IElement>(&element, [&](IElement::State& es) { es.world_matrix = world; });
}

} // namespace velk::ui
