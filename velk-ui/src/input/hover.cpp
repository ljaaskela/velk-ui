#include "hover.h"

#include <velk/api/state.h>

namespace velk_ui {

void Hover::on_pointer_enter(const PointerEvent&)
{
    velk::write_state<IHover>(this, [](IHover::State& s) { s.hovered = true; });
    velk::invoke_event(get_interface(velk::IInterface::UID), "on_hover_changed");
}

void Hover::on_pointer_leave(const PointerEvent&)
{
    velk::write_state<IHover>(this, [](IHover::State& s) { s.hovered = false; });
    velk::invoke_event(get_interface(velk::IInterface::UID), "on_hover_changed");
}

} // namespace velk_ui
