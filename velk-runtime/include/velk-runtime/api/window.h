#ifndef VELK_RUNTIME_API_WINDOW_H
#define VELK_RUNTIME_API_WINDOW_H

#include <velk/api/event.h>
#include <velk/api/object.h>

#include <velk-render/api/render_context.h>
#include <velk-ui/api/input_dispatcher.h>
#include <velk-runtime/interface/intf_window.h>

namespace velk {

/**
 * @brief Convenience wrapper around IWindow.
 *
 * Provides null-safe access to window operations.
 *
 *   auto window = app.create_window({.width = 1280, .height = 720});
 *   auto surface = window.surface();
 *   window.input().pointer_event(ev);
 */
class Window : public Object
{
public:
    Window() = default;
    explicit Window(IObject::Ptr obj) : Object(check_object<IWindow>(obj)) {}
    explicit Window(IWindow::Ptr w) : Object(as_object(w)) {}

    operator IWindow::Ptr() const { return as_ptr<IWindow>(); }

    /** @brief Returns the window size in pixels. */
    ::velk::size get_size() const { return read_state_value<IWindow>(&IWindow::State::size); }

    /** @brief Returns the render surface for this window. */
    ISurface::Ptr surface() const
    {
        return with<IWindow>([](auto& w) { return w.surface(); });
    }

    /** @brief Returns the input dispatcher for this window. */
    ui::IInputDispatcher* input() const
    {
        return with<IWindow>([](auto& w) -> ui::IInputDispatcher* { return &w.input(); });
    }

    template <class F>
    ScopedHandler add_on_pointer_event(F&& fn)
    {
        return with<IWindow>(
            [&](auto& w) { return ScopedHandler(w.input().on_pointer_event(), std::move(fn)); });
    }

    /** @brief Returns the render context associated with this window. */
    RenderContext render_context() const
    {
        return RenderContext(with<IWindow>([](auto& w) { return w.render_context(); }));
    }

    /** @brief Resize event. Handler receives ::velk::size new size. */
    Event on_resize() const
    {
        return with<IWindow>([](auto& w) { return w.on_resize(); });
    }

    /** @brief Fired when a new platform surface is attached. Handler receives the native handle. */
    Event on_surface_created() const
    {
        return with<IWindow>([](auto& w) { return w.on_surface_created(); });
    }

    /** @brief Fired when the existing surface dimensions change. Handler receives ::velk::size. */
    Event on_surface_changed() const
    {
        return with<IWindow>([](auto& w) { return w.on_surface_changed(); });
    }

    /** @brief Fired when the platform surface is destroyed (no arg). */
    Event on_surface_destroyed() const
    {
        return with<IWindow>([](auto& w) { return w.on_surface_destroyed(); });
    }

    /** @brief Returns true if the window has been requested to close. */
    bool should_close() const
    {
        return with<IWindow>([](auto& w) { return w.should_close(); });
    }
};

} // namespace velk

#endif // VELK_RUNTIME_API_WINDOW_H
