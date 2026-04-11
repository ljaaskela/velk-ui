#include "application.h"

#include <velk/api/velk.h>
#include <velk/interface/intf_plugin_registry.h>

#include <velk-render/api/render_context.h>
#include <velk-runtime/interface/intf_window.h>
#include <velk-ui/api/camera.h>
#include <velk-ui/api/element.h>
#include <velk-ui/api/renderer.h>
#include <velk-ui/api/scene.h>
#include <velk-ui/interface/intf_element.h>
#include <velk-ui/plugins/text/api/font.h>
#include <velk-ui/plugins/text/api/text_visual.h>

#include <cstdio>

namespace velk::impl {

bool Application::init(const ApplicationConfig& config)
{
    config_ = config;

    auto& v = instance();
    auto& reg = v.plugin_registry();

    // Load all standard plugins.
    reg.load_plugin_from_path("velk_ui.dll");
    reg.load_plugin_from_path("velk_render.dll");
    reg.load_plugin_from_path("velk_vk.dll");
    reg.load_plugin_from_path("velk_text.dll");
    reg.load_plugin_from_path("velk_image.dll");
    reg.load_plugin_from_path("velk_importer.dll");

    // Load the GLFW platform plugin and get its IWindowProvider.
    reg.load_plugin_from_path("velk_runtime_glfw.dll");
    glfw_plugin_ = reg.find_plugin(PluginId::RuntimeGlfwPlugin);
    if (!glfw_plugin_) {
        VELK_LOG(E, "Failed to load GLFW platform plugin");
        return false;
    }

    window_provider_ = interface_cast<IWindowProvider>(glfw_plugin_);
    if (!window_provider_) {
        VELK_LOG(E, "GLFW plugin does not implement IWindowProvider");
        return false;
    }

    return true;
}

IObject::Ptr Application::create_window(const WindowConfig& config)
{
    if (!window_provider_) {
        return {};
    }

    // First window: render context does not exist yet.
    // Create the native window first (without a surface), then use its
    // backend params to create the render context, then finalize the surface.
    if (!render_ctx_) {
        auto win_obj = window_provider_->create_window(config, nullptr);
        if (!win_obj) {
            VELK_LOG(E, "Failed to create first window");
            return {};
        }

        // Create render context using the platform backend params.
        void* params = window_provider_->get_backend_params();
        RenderConfig render_config;
        render_config.backend = config_.backend;
        render_config.backend_params = params;
        render_ctx_ = create_render_context(render_config);
        if (!render_ctx_) {
            VELK_LOG(E, "Failed to create render context");
            return {};
        }

        // Create the renderer.
        renderer_ = ui::create_renderer(*render_ctx_);
        if (!renderer_) {
            VELK_LOG(E, "Failed to create renderer");
            return {};
        }

        // Finalize the window: create surface and assign it.
        window_provider_->finalize_window(win_obj, render_ctx_);

        windows_.emplace_back(std::move(win_obj));
        return windows_.back();
    }

    // Subsequent windows: render context exists, pass it to the provider.
    auto win_obj = window_provider_->create_window(config, render_ctx_);
    if (!win_obj) {
        VELK_LOG(E, "Failed to create window");
        return {};
    }
    windows_.emplace_back(std::move(win_obj));
    return windows_.back();
}

void Application::add_view(const IObject::Ptr& window,
                           const ui::IElement::Ptr& camera,
                           const rect& viewport)
{
    if (!renderer_) {
        return;
    }
    auto* iwin = interface_cast<IWindow>(window);
    if (!iwin) {
        return;
    }
    renderer_->add_view(camera, iwin->surface(), viewport);

    // Derive the scene from the camera element and bind resize.
    auto* elem = interface_cast<ui::IElement>(camera);
    if (elem) {
        auto scene = elem->get_scene();
        if (scene) {
            bind_scene_to_window(window, iwin, scene);

            // Also bind the window's input dispatcher to this scene.
            iwin->input().set_scene(scene);
        }
    }
}

void Application::bind_scene_to_window(const IObject::Ptr& window,
                                       IWindow* iwin,
                                       const shared_ptr<ui::IScene>& scene)
{
    // Check if this window-scene pair is already linked.
    for (auto& link : scene_links_) {
        auto* existing = interface_cast<IWindow>(link.window);
        if (existing == iwin && link.scene == scene) {
            return;
        }
    }

    // Set initial scene geometry from window size.
    auto sz = read_state<IWindow>(iwin)->size;
    scene->set_geometry(aabb::from_size(sz));

    // Subscribe to on_resize to update scene geometry synchronously
    // inside the platform event loop (matching old GLFW callback timing).
    weak_ptr<ui::IScene> weak_scene(scene);
    ScopedHandler resize_handler(iwin->on_resize(), [weak_scene](const ::velk::size& new_size) {
        auto s = weak_scene.lock();
        if (s) {
            s->set_geometry(aabb::from_size(new_size));
        }
    });

    WindowSceneLink link;
    link.window = window;
    link.scene = scene;
    link.resize_handler = std::move(resize_handler);
    scene_links_.emplace_back(std::move(link));
}

bool Application::poll()
{
    if (!window_provider_) {
        return false;
    }
    return window_provider_->poll_events();
}

void Application::update()
{
    instance().update();
}

void Application::present()
{
    if (!renderer_) {
        return;
    }

    using clock = std::chrono::steady_clock;
    auto start = clock::now();
    auto frame = renderer_->prepare();
    auto cpu_end = clock::now();
    renderer_->present(frame);
    auto frame_end = clock::now();

    double cpu_time = std::chrono::duration<double>(cpu_end - start).count();
    double frame_time = std::chrono::duration<double>(frame_end - start).count();

    // Update performance overlays.
    for (auto& ov : overlays_) {
        ov.frame_count++;
        ov.cpu_time_accum += cpu_time;
        ov.frame_time_accum += frame_time;

        double elapsed = std::chrono::duration<double>(frame_end - ov.last_update).count();
        if (elapsed >= 0.5) {
            double fps = static_cast<double>(ov.frame_count) / elapsed;
            double avg_cpu_ms = ov.cpu_time_accum / static_cast<double>(ov.frame_count) * 1000.0;
            double avg_frame_ms = ov.frame_time_accum / static_cast<double>(ov.frame_count) * 1000.0;
            char buf[96];
            std::snprintf(buf, sizeof(buf), "%.0f fps  cpu %.3f ms  frame %.3f ms",
                          fps, avg_cpu_ms, avg_frame_ms);
            ov.text_visual.set_text(string(buf));
            ov.frame_count = 0;
            ov.cpu_time_accum = 0.0;
            ov.frame_time_accum = 0.0;
            ov.last_update = frame_end;
        }
    }
}

IRenderContext::Ptr Application::render_context() const
{
    return render_ctx_;
}

ui::IRenderer::Ptr Application::renderer() const
{
    return renderer_;
}

void Application::set_performance_overlay(const IObject::Ptr& window,
                                          const PerformanceOverlayConfig& config)
{
    // Find existing overlay for this window.
    auto it = overlays_.begin();
    for (; it != overlays_.end(); ++it) {
        if (it->window == window) {
            break;
        }
    }

    if (!config.enabled && it != overlays_.end()) {
        overlays_.erase(it);
        return;
    }

    auto* iwin = interface_cast<IWindow>(window);
    if (!iwin || !renderer_) {
        return;
    }

    // Create the overlay scene with a root element, an ortho camera child,
    // and a text element child holding the FPS text visual.
    auto scene = ui::create_scene();
    auto root = ui::create_element();
    scene.set_root(root);

    auto camera_elem = ui::create_element();
    scene.add(root, camera_elem);

    auto camera_trait = ui::create_camera();
    camera_trait.set_projection(ui::Projection::Ortho);
    camera_elem.add_trait(camera_trait);

    auto text_elem = ui::create_element();
    scene.add(root, text_elem);

    auto text_visual = ui::visual::create_text();
    text_visual.set_font(ui::get_default_font());
    text_visual.set_text("FPS: ...");
    text_visual.set_font_size(config.font_size);
    text_visual.set_color(config.text_color);
    text_elem.add_trait(text_visual);

    // Set initial scene geometry from window size.
    auto win_size = read_state<IWindow>(iwin)->size;
    scene.set_geometry(aabb::from_size(win_size));

    // Add the overlay as a view on the window's surface.
    // Added last so it's drawn on top of any existing user views.
    renderer_->add_view(camera_elem, iwin->surface(), {0, 0, 1, 1});

    // Bind resize so the overlay scene tracks window size.
    bind_scene_to_window(window, iwin, static_cast<ui::IScene::Ptr>(scene));

    PerformanceOverlay ov;
    ov.window = window;
    ov.scene = std::move(scene);
    ov.camera_element = std::move(camera_elem);
    ov.text_element = std::move(text_elem);
    ov.text_visual = std::move(text_visual);
    ov.last_update = std::chrono::steady_clock::now();

    if (it != overlays_.end()) {
        *it = std::move(ov);
    } else {
        overlays_.emplace_back(std::move(ov));
    }
}

void Application::shutdown()
{
    overlays_.clear();
    scene_links_.clear();
    windows_.clear();
    if (renderer_) {
        renderer_->shutdown();
        renderer_ = nullptr;
    }
    render_ctx_ = nullptr;
    window_provider_ = nullptr;
    glfw_plugin_ = nullptr;
}

} // namespace velk::impl
