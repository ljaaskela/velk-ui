// Fluent RT demo — landing pad for the calendar-of-mirrors plan in
// design-notes/fluent_demo.md. Boots a window, loads the scene,
// flips the scene's first (and only) camera into RenderPath::RayTrace,
// and wires mouse orbit + scroll zoom for iteration.
//
// The scene itself is deliberately simple until step 7 lands.

#include <velk/api/any.h>
#include <velk/api/callback.h>
#include <velk/api/velk.h>

#include <velk-render/api/render_context.h>
#include <velk-runtime/api/application.h>
#include <velk-ui/api/camera.h>
#include <velk-ui/api/element.h>
#include <velk-ui/api/scene.h>
#include <velk-ui/api/trait/orbit.h>
#include <velk-ui/interface/intf_camera.h>

int main(int /*argc*/, char* /*argv*/[])
{
    velk::ApplicationConfig config;

    constexpr int kWidth = 1280;
    constexpr int kHeight = 720;

    velk::WindowConfig wc;
    wc.width = kWidth;
    wc.height = kHeight;
    wc.title = "velk-ui fluent";

    auto app = velk::create_app(config);
    auto window = app.create_window(wc);
    if (!window) {
        VELK_LOG(E, "Initialization failed");
        return 1;
    }

    // Optional Tracy profiler plugin; ignored if not present.
    velk::instance().plugin_registry().load_plugin_from_path("velk_tracy.dll");

    auto scene = velk::ui::create_scene("app://scenes/fluent.json");
    scene.set_geometry(velk::aabb::from_size({static_cast<float>(kWidth), static_cast<float>(kHeight)}));

    // The fluent scene has a single perspective camera with an orbit trait.
    // Flip it to ray-traced. If the scene isn't fully populated yet, the
    // camera may not exist — the RT path just draws the env background.
    auto camera_3d = scene.find_first<velk::ui::IOrbit>();

    velk::ui::OrbitTrait orbit;
    if (camera_3d) {
        velk::ui::Camera(camera_3d.find_trait<velk::ui::ICamera>())
            .set_render_path(velk::ui::RenderPath::RayTrace);
        app.add_view(window, camera_3d, {0, 0, 1.f, 1.f});
        orbit = velk::ui::OrbitTrait(camera_3d.find_trait<velk::ui::IOrbit>());
    } else {
        VELK_LOG(E, "Fluent scene has no orbit camera yet; window will be blank.");
    }

    // Orbit + zoom input wiring (same pattern as the simple sample).
    bool orbit_dragging = false;
    double orbit_last_x = 0.0, orbit_last_y = 0.0;

    velk::ScopedHandler pointer_sub = window.add_on_pointer_event([&](const velk::ui::PointerEvent& e) {
        if (e.action == velk::ui::PointerAction::Down && e.button == velk::ui::PointerButton::Right && orbit) {
            orbit_dragging = true;
            orbit_last_x = e.position.x;
            orbit_last_y = e.position.y;
        } else if (e.action == velk::ui::PointerAction::Up && e.button == velk::ui::PointerButton::Right) {
            orbit_dragging = false;
        } else if (e.action == velk::ui::PointerAction::Move && orbit_dragging && orbit) {
            float dx = e.position.x - static_cast<float>(orbit_last_x);
            float dy = e.position.y - static_cast<float>(orbit_last_y);
            auto state = velk::read_state<velk::ui::IOrbit>(static_cast<velk::ui::IOrbit::Ptr>(orbit));
            if (state) {
                orbit.set_yaw(state->yaw + dx * 0.3f);
                orbit.set_pitch(state->pitch + dy * 0.3f);
            }
            orbit_last_x = e.position.x;
            orbit_last_y = e.position.y;
        }
    });

    auto* dispatcher = window.input();
    velk::ScopedHandler scroll_sub(dispatcher->on_scroll_event(),
        [&](const velk::ui::ScrollEvent& e) {
            if (!orbit) return;
            auto state =
                velk::read_state<velk::ui::IOrbit>(static_cast<velk::ui::IOrbit::Ptr>(orbit));
            if (state) {
                float factor = (e.delta.y > 0) ? 0.9f : 1.1f;
                orbit.set_distance(state->distance * factor);
            }
        });

    velk::PerformanceOverlayConfig poc;
    poc.enabled = true;
    app.set_performance_overlay(window, poc);

    app.update();
    app.present();

    while (app.poll()) {
        app.update();
        app.present();
    }
    return 0;
}
