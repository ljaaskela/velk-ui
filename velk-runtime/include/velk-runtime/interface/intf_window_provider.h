#ifndef VELK_RUNTIME_INTF_WINDOW_PROVIDER_H
#define VELK_RUNTIME_INTF_WINDOW_PROVIDER_H

#include <velk/interface/intf_interface.h>

#include <velk-render/interface/intf_render_context.h>
#include <velk-runtime/interface/intf_application.h>
#include <velk-runtime/interface/intf_window.h>

namespace velk {

/**
 * @brief Platform backend extension point for window creation.
 *
 * Implemented by platform plugins (e.g. GLFW, Android).
 * The Application queries this interface to create native windows.
 */
class IWindowProvider : public Interface<IWindowProvider>
{
public:
    /**
     * @brief Creates a new native window owned by the platform layer.
     *
     * If ctx is non-null, creates a surface from it and the window is
     * fully initialized. If ctx is null, the window is created without
     * a surface. Call finalize_window() after creating the render context.
     *
     * Returns null if this provider does not support creating native windows.
     */
    virtual IWindow::Ptr create_window(const WindowConfig& config,
                                       const IRenderContext::Ptr& ctx) = 0;

    /**
     * @brief Wraps an externally-provided platform surface (e.g. HWND, ANativeWindow*).
     *
     * Dimensions are queried from the native handle. The user is responsible for
     * feeding input events and notifying the window of resizes via on_resize.
     *
     * Returns null if this provider does not support wrapping native surfaces
     * for the given handle type.
     */
    virtual IWindow::Ptr wrap_native_surface(void* native_handle,
                                             const IRenderContext::Ptr& ctx) = 0;

    /**
     * @brief Assigns a surface and render context to a window created without one.
     *
     * Used for the first window, which is created before the render context
     * exists (to provide backend params).
     */
    virtual void finalize_window(const IWindow::Ptr& window,
                                 const IRenderContext::Ptr& ctx) = 0;

    /** @brief Polls all platform events. Returns false if quit was requested. */
    virtual bool poll_events() = 0;

    /**
     * @brief Returns backend-specific init params for render context creation.
     *
     * Must be called after at least one window has been created.
     * The returned pointer is owned by the provider and valid until shutdown.
     */
    virtual void* get_backend_params() = 0;
};

} // namespace velk

#endif // VELK_RUNTIME_INTF_WINDOW_PROVIDER_H
