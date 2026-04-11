#ifndef VELK_RUNTIME_INTF_WINDOW_H
#define VELK_RUNTIME_INTF_WINDOW_H

#include <velk/interface/intf_metadata.h>

#include <velk-render/interface/intf_render_context.h>
#include <velk-render/interface/intf_surface.h>
#include <velk-ui/interface/intf_input_dispatcher.h>

namespace velk {

/** @brief A window or render target with input support. */
class IWindow : public Interface<IWindow>
{
public:
    VELK_INTERFACE(
        (RPROP, ::velk::size, size, {}),
        (EVT, on_resize)
    )

    /** @brief Returns the render surface for this window. */
    virtual ISurface::Ptr surface() const = 0;

    /** @brief Returns the input dispatcher bound to this window. */
    virtual ui::IInputDispatcher& input() const = 0;

    /** @brief Returns the render context (impl stores weak, locks on access). */
    virtual IRenderContext::Ptr render_context() const = 0;

    /** @brief Returns true if the window has been requested to close. */
    virtual bool should_close() const = 0;
};

} // namespace velk

#endif // VELK_RUNTIME_INTF_WINDOW_H
