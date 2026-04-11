#ifndef VELK_RUNTIME_API_APPLICATION_H
#define VELK_RUNTIME_API_APPLICATION_H

#include <velk/api/object.h>
#include <velk/api/velk.h>

#include <velk-render/api/render_context.h>
#include <velk-runtime/api/window.h>
#include <velk-runtime/interface/intf_application.h>
#include <velk-runtime/plugin.h>
#include <velk-ui/api/element.h>

namespace velk {

/**
 * @brief Convenience wrapper around IApplication.
 *
 * Provides null-safe access to the application runtime.
 *
 *   auto app = velk::create_app({});
 *   auto window = app.create_window({.width = 1280, .height = 720});
 *   while (app.poll()) {
 *       app.update();
 *       app.present();
 *   }
 */
class Application : public Object
{
public:
    Application() = default;
    explicit Application(IObject::Ptr obj) : Object(check_object<IApplication>(obj)) {}
    explicit Application(IApplication::Ptr a) : Object(as_object(a)) {}

    operator IApplication::Ptr() const { return as_ptr<IApplication>(); }

    /** @brief Creates a managed window with the given configuration. */
    Window create_window(const WindowConfig& config = {})
    {
        return Window(with<IApplication>([&](auto& a) { return a.create_window(config); }));
    }

    /** @brief Binds a camera to a window's surface for rendering. */
    void add_view(const IWindow::Ptr& window, const ui::IElement::Ptr& camera, const rect& viewport = {})
    {
        with<IApplication>(
            [&](auto& a) { a.add_view(interface_pointer_cast<::velk::IObject>(window), camera, viewport); });
    }

    /** @brief Pumps platform events. Returns false when all windows are closed. */
    bool poll()
    {
        return with<IApplication>([](auto& a) { return a.poll(); });
    }

    /** @brief Runs velk update and scene processing. */
    void update()
    {
        with<IApplication>([](auto& a) { a.update(); });
    }

    /** @brief Presents all windows. */
    void present()
    {
        with<IApplication>([](auto& a) { a.present(); });
    }

    /** @brief Returns the render context owned by this application. */
    RenderContext render_context() const
    {
        return RenderContext(with<IApplication>([](auto& a) { return a.render_context(); }));
    }

    /** @brief Returns the default renderer. */
    ui::IRenderer::Ptr renderer() const
    {
        return with<IApplication>([](auto& a) { return a.renderer(); });
    }

    /** @brief Enables or configures a performance overlay on a window. */
    void set_performance_overlay(const Window& window,
                                 const PerformanceOverlayConfig& config = {})
    {
        with<IApplication>([&](auto& a) {
            a.set_performance_overlay(static_cast<IObject::Ptr>(window), config);
        });
    }
};

/**
 * @brief Creates and initializes an Application with the given config.
 *
 * Loads the runtime plugin, creates an IApplication, and initializes it
 * (which in turn loads all standard plugins and creates the render context).
 */
inline Application create_app(const ApplicationConfig& config = {})
{
    auto& v = instance();
    v.plugin_registry().load_plugin_from_path("velk_runtime.dll");
    auto app = v.create<IApplication>(ClassId::Application);
    return Application(app && app->init(config) ? std::move(app) : nullptr);
}

} // namespace velk

#endif // VELK_RUNTIME_API_APPLICATION_H
