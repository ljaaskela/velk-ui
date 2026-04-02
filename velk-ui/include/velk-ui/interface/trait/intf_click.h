#ifndef VELK_UI_INTF_CLICK_H
#define VELK_UI_INTF_CLICK_H

#include <velk/interface/intf_metadata.h>

namespace velk_ui {

/**
 * @brief Input trait that detects pointer click gestures.
 *
 * Fires on_click when a pointer down + up sequence completes within the element.
 * Exposes a read-only pressed property for visual feedback during the press.
 */
class IClick : public velk::Interface<IClick>
{
public:
    VELK_INTERFACE(
        (RPROP, bool, pressed, false), ///< True while the pointer is down on this element.
        (EVT, on_click)                ///< Fired on pointer up after a successful press.
    )
};

} // namespace velk_ui

#endif // VELK_UI_INTF_CLICK_H
