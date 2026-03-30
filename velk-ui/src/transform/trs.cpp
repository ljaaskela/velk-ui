#include "trs.h"

#include <velk/api/state.h>
#include <velk-ui/interface/intf_element.h>

namespace velk_ui {

static constexpr float deg_to_rad(float deg) { return deg * 3.14159265358979323846f / 180.f; }

void Trs::transform(IElement& element)
{
    auto state = velk::read_state<ITrs>(this);
    if (!state) {
        return;
    }

    velk::mat4 t = velk::mat4::translate(state->translate);
    velk::mat4 r = velk::mat4::rotate_z(deg_to_rad(state->rotation));
    velk::mat4 s = velk::mat4::scale({state->scale.x, state->scale.y, 1.f});
    velk::mat4 trs = t * r * s;

    auto elem_state = velk::read_state<IElement>(&element);
    if (!elem_state) {
        return;
    }

    velk::mat4 world = elem_state->world_matrix * trs;
    velk::write_state<IElement>(&element, [&](IElement::State& es) { es.world_matrix = world; });
}

} // namespace velk_ui
