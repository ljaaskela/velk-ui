#include "drag.h"

#include <velk/api/event.h>
#include <velk/api/state.h>

namespace velk::ui {

DragEvent Drag::make_drag_event(const PointerEvent& event) const
{
    DragEvent d;
    d.start_position = start_position_;
    d.position = event.local_position;
    d.delta = event.local_position - last_position_;
    d.total_delta = event.local_position - start_position_;
    d.button = event.button;
    d.modifiers = event.modifiers;
    return d;
}

InputResult Drag::on_pointer_event(PointerEvent& event)
{
    if (event.action == PointerAction::Down) {
        start_position_ = event.local_position;
        last_position_ = event.local_position;
        active_ = true;
        return InputResult::Captured;
    }

    if (!active_) {
        return InputResult::Ignored;
    }

    if (event.action == PointerAction::Move) {
        auto reader = read_state<IDrag>(this);
        bool starting = reader && !reader->dragging;
        if (starting) {
            set_dragging(true);
            invoke_event(get_interface(IInterface::UID), "on_drag_start", make_drag_event(event));
        } else {
            invoke_event(get_interface(IInterface::UID), "on_drag_move", make_drag_event(event));
        }
        last_position_ = event.local_position;
        return InputResult::Consumed;
    }

    if (event.action == PointerAction::Up || event.action == PointerAction::Cancel) {
        auto reader = read_state<IDrag>(this);
        if (reader && reader->dragging) {
            invoke_event(get_interface(IInterface::UID), "on_drag_end", make_drag_event(event));
            set_dragging(false);
        }
        active_ = false;
        return InputResult::Consumed;
    }

    return InputResult::Ignored;
}

void Drag::set_dragging(bool v)
{
    write_state<IDrag>(this, [v](IDrag::State& s) { s.dragging = v; });
}

} // namespace velk::ui
