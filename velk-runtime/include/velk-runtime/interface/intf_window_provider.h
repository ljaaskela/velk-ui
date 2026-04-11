#ifndef VELK_RUNTIME_INTF_WINDOW_PROVIDER_H
#define VELK_RUNTIME_INTF_WINDOW_PROVIDER_H

#include <velk/interface/intf_interface.h>

#include <velk-render/interface/intf_render_context.h>
#include <velk-runtime/interface/intf_application.h>

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
     * @brief Creates a native window.
     *
     * If ctx is non-null, creates a surface from it and the window is
     * fully initialized. If ctx is null, the window is created without
     * a surface. Call finalize_window() after creating the render context.
     */
    virtual IObject::Ptr create_window(const WindowConfig& config,
                                       const IRenderContext::Ptr& ctx) = 0;

    /**
     * @brief Assigns a surface and render context to a window created without one.
     *
     * Used for the first window, which is created before the render context
     * exists (to provide backend params).
     */
    virtual void finalize_window(const IObject::Ptr& window,
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
