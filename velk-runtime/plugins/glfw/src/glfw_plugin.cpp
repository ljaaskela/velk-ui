#include "glfw_plugin.h"

#include "glfw_window.h"

#include <velk/api/velk.h>

#include <velk-ui/api/input_dispatcher.h>

namespace velk::impl {

static void glfw_error_callback(int error, const char* description)
{
    VELK_LOG(E, "GLFW error %d: %s", error, description);
}

ReturnValue GlfwPlugin::initialize(IVelk& velk, PluginConfig&)
{
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) {
        VELK_LOG(E, "Failed to initialize GLFW");
        return ReturnValue::Fail;
    }

    return ::velk::register_type<GlfwWindow>(velk);
}

ReturnValue GlfwPlugin::shutdown(IVelk&)
{
    windows_.clear();
    glfwTerminate();
    return ReturnValue::Success;
}

GlfwWindow* GlfwPlugin::get_glfw_window(const IObject::Ptr& obj) const
{
    auto* iwin = interface_cast<IWindow>(obj);
    if (!iwin) {
        return nullptr;
    }
    // GlfwWindow stores itself as the GLFW user pointer.
    auto surface = iwin->surface();
    // Actually, we get it through the native handle lookup.
    // Since GlfwWindow sets glfwSetWindowUserPointer(window_, this),
    // we need the GLFWwindow*. But we don't have it from IWindow alone.
    // Use static_cast since we know the concrete type within this plugin.
    return static_cast<GlfwWindow*>(iwin);
}

IObject::Ptr GlfwPlugin::create_window(const WindowConfig& config,
                                       const IRenderContext::Ptr& ctx)
{
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, config.resizable ? GLFW_TRUE : GLFW_FALSE);

    GLFWwindow* glfw_win = glfwCreateWindow(
        config.width, config.height,
        string(config.title).c_str(),
        nullptr, nullptr);

    if (!glfw_win) {
        VELK_LOG(E, "Failed to create GLFW window");
        return {};
    }

    // Create the velk window object.
    auto obj = instance().create<IObject>(ClassId::GlfwWindow);
    if (!obj) {
        glfwDestroyWindow(glfw_win);
        return {};
    }

    auto* win = get_glfw_window(obj);
    if (!win) {
        glfwDestroyWindow(glfw_win);
        return {};
    }

    win->set_glfw_handle(glfw_win);

    // Set initial size.
    write_state<IWindow>(win, [&](IWindow::State& s) {
        s.size = {static_cast<float>(config.width), static_cast<float>(config.height)};
    });

    // Create input dispatcher (not bound to a scene yet; user binds via scene).
    auto dispatcher = instance().create<ui::IInputDispatcher>(ui::ClassId::Input::Dispatcher);
    win->set_input_dispatcher(std::move(dispatcher));

    // If render context is available, create the surface now.
    if (ctx) {
        auto surface = ctx->create_surface(config.width, config.height);
        win->set_surface(std::move(surface));
        win->set_render_context(ctx);
    } else {
        // First window: set up VulkanInitParams for later render context creation.
        vk_params_.user_data = glfw_win;
        vk_params_.create_surface = [](void* vk_instance, void* out_surface, void* user_data) -> bool {
            auto inst = static_cast<VkInstance>(vk_instance);
            auto* surf = static_cast<VkSurfaceKHR*>(out_surface);
            auto* w = static_cast<GLFWwindow*>(user_data);
            return glfwCreateWindowSurface(inst, w, nullptr, surf) == VK_SUCCESS;
        };
    }

    windows_.emplace_back(obj);
    return obj;
}

void GlfwPlugin::finalize_window(const IObject::Ptr& window,
                                 const IRenderContext::Ptr& ctx)
{
    if (!ctx) {
        return;
    }

    auto* win = get_glfw_window(window);
    if (!win) {
        return;
    }

    auto state = read_state<IWindow>(win);
    int w = static_cast<int>(state->size.width);
    int h = static_cast<int>(state->size.height);

    auto surface = ctx->create_surface(w, h);
    win->set_surface(std::move(surface));
    win->set_render_context(ctx);
}

bool GlfwPlugin::poll_events()
{
    glfwPollEvents();

    // Return false if all windows should close.
    for (auto& obj : windows_) {
        auto* win = interface_cast<IWindow>(obj);
        if (win && !win->should_close()) {
            return true;
        }
    }
    return false;
}

void* GlfwPlugin::get_backend_params()
{
    return &vk_params_;
}

} // namespace velk::impl
