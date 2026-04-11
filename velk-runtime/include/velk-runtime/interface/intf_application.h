#ifndef VELK_RUNTIME_INTF_APPLICATION_H
#define VELK_RUNTIME_INTF_APPLICATION_H

#include <velk/interface/intf_interface.h>

#include <velk-render/interface/intf_render_context.h>
#include <velk-render/render_types.h>
#include <velk-runtime/interface/intf_window.h>
#include <velk-ui/interface/intf_element.h>
#include <velk-ui/interface/intf_renderer.h>

namespace velk {

/** @brief Window creation parameters. */
struct WindowConfig
{
    int width{1280};
    int height{720};
    string_view title{"velk"};
    bool resizable{true};
};

/** @brief Application initialization parameters. */
struct ApplicationConfig
{
    RenderBackendType backend{RenderBackendType::Default};
    string_view app_root;
    string_view assets_root;
};

/** @brief Performance overlay configuration. */
struct PerformanceOverlayConfig
{
    bool enabled{true};
    float font_size{14.f};
    color text_color{1.f, 1.f, 0.f, 1.f};
};

/**
 * @brief Application runtime interface.
 *
 * Owns the renderer, manages windows and the frame loop.
 * Created via velk::create_app().
 */
class IApplication : public Interface<IApplication>
{
public:
    /** @brief Initializes the runtime: loads plugins, creates render context. */
    virtual bool init(const ApplicationConfig& config) = 0;

    /** @brief Creates a new managed native window (desktop). */
    virtual IWindow::Ptr create_window(const WindowConfig& config) = 0;

    /**
     * @brief Wraps an externally-provided platform surface.
     *
     * Used for framework-driven cases (Android ANativeWindow*, embedded HWND, etc.).
     * Dimensions are queried from the native handle. The user is responsible for
     * feeding input events to window.input() and notifying the window of resizes.
     */
    virtual IWindow::Ptr wrap_native_surface(void* native_handle) = 0;

    /** @brief Binds a camera to a window's surface for rendering. */
    virtual void add_view(const IObject::Ptr& window,
                          const ui::IElement::Ptr& camera,
                          const rect& viewport = {}) = 0;

    /** @brief Pumps platform events. Returns false when all windows are closed. */
    virtual bool poll() = 0;

    /** @brief Runs velk update and scene processing. */
    virtual void update() = 0;

    /** @brief Prepares a frame on the calling thread (main/UI thread). */
    virtual ui::Frame prepare() = 0;

    /** @brief Submits a prepared frame (safe from any thread, typically render thread). */
    virtual void submit(ui::Frame frame) = 0;

    /** @brief Convenience: prepare() + submit() in one call (synchronous, single-threaded). */
    virtual void present() = 0;

    /** @brief Returns the render context owned by this application. */
    virtual IRenderContext::Ptr render_context() const = 0;

    /** @brief Returns the default renderer owned by this application. */
    virtual ui::IRenderer::Ptr renderer() const = 0;

    /**
     * @brief Enables or configures a performance overlay on a window.
     *
     * The overlay shows FPS, CPU and frame time. It is rendered as a separate
     * scene drawn on top of any existing views on the window's surface.
     */
    virtual void set_performance_overlay(const IObject::Ptr& window,
                                         const PerformanceOverlayConfig& config) = 0;

    /** @brief Shuts down the application, releasing all resources. */
    virtual void shutdown() = 0;
};

} // namespace velk

#endif // VELK_RUNTIME_INTF_APPLICATION_H
