#include <velk/api/object_ref.h>
#include <velk/api/velk.h>
#include <velk/interface/intf_plugin_registry.h>

#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <velk-ui/api/constraint/fixed_size.h>
#include <velk-ui/api/element.h>
#include <velk-ui/api/material/shader.h>
#include <velk-ui/api/scene.h>
#include <velk-ui/api/visual/rect.h>
#include <velk-ui/interface/intf_renderer.h>
#include <velk-ui/plugins/render/plugin.h>
#include <velk-ui/plugins/text/api/font.h>
#include <velk-ui/plugins/text/api/text_visual.h>

static void glfw_error_callback(int error, const char* description)
{
    VELK_LOG(E, "GLFW error %d: %s", error, description);
}

static velk_ui::IRenderer* g_renderer = nullptr;
static velk_ui::Scene* g_scene = nullptr;
static velk_ui::ISurface::Ptr g_surface;

static void glfw_framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    if (g_scene) {
        g_scene->set_geometry(velk::aabb::from_size({
            static_cast<float>(width), static_cast<float>(height)}));
    }
    if (g_surface) {
        velk::write_state<velk_ui::ISurface>(g_surface, [&](velk_ui::ISurface::State& s) {
            s.width = width;
            s.height = height;
        });
    }
}

int main(int argc, char* argv[])
{
    glfwSetErrorCallback(glfw_error_callback);

    if (!glfwInit()) {
        VELK_LOG(E, "Failed to initialize GLFW");
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    constexpr int kWidth = 1280;
    constexpr int kHeight = 720;

    GLFWwindow* window = glfwCreateWindow(kWidth, kHeight, "velk-ui", nullptr, nullptr);
    if (!window) {
        VELK_LOG(E, "Failed to create GLFW window");
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);

    if (!gladLoadGL(reinterpret_cast<GLADloadfunc>(glfwGetProcAddress))) {
        VELK_LOG(E, "Failed to load OpenGL via GLAD2");
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    VELK_LOG(I, "OpenGL %s", reinterpret_cast<const char*>(glGetString(GL_VERSION)));

    glfwSwapInterval(1); // vsync

    auto& velk = velk::instance();

    // Load plugins
    velk.plugin_registry().load_plugin_from_path("velk_ui.dll");
    velk.plugin_registry().load_plugin_from_path("velk_render.dll");
    velk.plugin_registry().load_plugin_from_path("velk_gl.dll");
    velk.plugin_registry().load_plugin_from_path("velk_text.dll");
    velk.plugin_registry().load_plugin_from_path("velk_importer.dll");

    // Create and init renderer
    auto renderer_obj = velk.create<velk_ui::IRenderer>(velk_ui::ClassId::Renderer);
    if (!renderer_obj) {
        VELK_LOG(E, "Failed to create renderer");
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    velk_ui::RenderConfig config{velk_ui::RenderBackendType::GL};
    if (!renderer_obj->init(config)) {
        VELK_LOG(E, "Failed to initialize renderer");
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    // Create surface and scene
    auto surface = renderer_obj->create_surface(kWidth, kHeight);

    auto scene = velk_ui::create_scene("app://scenes/stack_test.json");
    scene.set_geometry(velk::aabb::from_size({
        static_cast<float>(kWidth), static_cast<float>(kHeight)}));

    renderer_obj->attach(surface, static_cast<velk_ui::IScene::Ptr>(scene));

    g_renderer = renderer_obj.get();
    g_scene = &scene;
    g_surface = surface;
    glfwSetFramebufferSizeCallback(window, glfw_framebuffer_size_callback);

    auto root = scene.root();
    auto child3 = scene.child_at(root, 2);
    // Apply a gradient shader to the blue rect (child3)
    {
        auto visual = child3.find_trait<velk_ui::IVisual>();
        if (visual) {
            auto mat = velk_ui::material::create_shader();
            mat.set_fragment_source(R"(
                #version 330 core
                in vec4 v_color;
                out vec4 frag_color;
                uniform vec4 u_rect;
                void main()
                {
                    float t = (gl_FragCoord.x - u_rect.x) / max(u_rect.z, 1.0);
                    vec3 top = vec3(0.1, 0.4, 0.9);
                    vec3 bot = vec3(0.9, 0.2, 0.6);
                    frag_color = vec4(mix(top, bot, t), 1.0);
                }
            )");
            // Set paint via state write
            velk::write_state<velk_ui::IVisual>(
                visual, [&](velk_ui::IVisual::State& s) { s.paint = velk::create_object_ref(mat.get()); });
            VELK_LOG(I, "Paint set on child2, mat=%p", static_cast<void*>(mat.get().get()));
        }
    }

    // Programmatically create a text element with "Hello, Velk!"
    {
        auto font = velk_ui::create_font();
        if (font.init_default() && font.set_size(32.f)) {
            auto tv = velk_ui::visual::create_text();
            tv.set_font(font);
            tv.set_text("Hello, Velk!");
            tv.set_color(velk::color::white());

            auto text_elem = velk_ui::create_element();

            child3.add_trait(tv);

            /*auto fs = velk_ui::constraint::create_fixed_size();
            fs.set_size(velk_ui::dim::px(400.f), velk_ui::dim::px(50.f));

            text_elem.add_trait(fs);
            text_elem.add_trait(tv);

            scene.add(scene.root(), text_elem);

            VELK_LOG(I, "Text element added: \"Hello, Velk!\"");*/
        }
    }

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        velk.update();
        renderer_obj->render();
        glfwSwapBuffers(window);
    }

    g_renderer = nullptr;
    g_scene = nullptr;
    g_surface = nullptr;

    scene = velk_ui::Scene{};
    renderer_obj->shutdown();
    renderer_obj = nullptr;

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
