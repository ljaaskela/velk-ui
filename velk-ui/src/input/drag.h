#ifndef VELK_UI_INPUT_DRAG_H
#define VELK_UI_INPUT_DRAG_H

#include <velk/api/math_types.h>

#include <velk-ui/ext/input.h>
#include <velk-ui/interface/trait/intf_drag.h>
#include <velk-ui/plugin.h>

namespace velk_ui {

class Drag : public ext::Input<Drag, IDrag>
{
public:
    VELK_CLASS_UID(ClassId::Input::Drag, "Drag");

    InputResult on_pointer_event(PointerEvent& event) override;

private:
    void set_dragging(bool v);

    velk::vec2 start_position_{};
    velk::vec2 last_position_{};
    bool active_ = false;
};

} // namespace velk_ui

#endif // VELK_UI_INPUT_DRAG_H
