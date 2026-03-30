#include "matrix.h"

#include <velk/api/state.h>
#include <velk-ui/interface/intf_element.h>

namespace velk_ui {

void Matrix::transform(IElement& element)
{
    auto state = velk::read_state<IMatrix>(this);
    if (!state) {
        return;
    }

    auto elem_state = velk::read_state<IElement>(&element);
    if (!elem_state) {
        return;
    }

    velk::mat4 world = elem_state->world_matrix * state->matrix;
    velk::write_state<IElement>(&element, [&](IElement::State& es) { es.world_matrix = world; });
}

} // namespace velk_ui
