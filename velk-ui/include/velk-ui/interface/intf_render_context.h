#ifndef VELK_UI_INTF_RENDER_CONTEXT_H
#define VELK_UI_INTF_RENDER_CONTEXT_H

#include <velk/interface/intf_metadata.h>

#include <velk-ui/interface/intf_renderer.h>
#include <velk-ui/interface/intf_surface.h>
#include <velk-ui/types.h>

namespace velk_ui {

/**
 * @brief Owns the render backend and creates renderers and surfaces.
 *
 * The context is created via create_render_context(). It loads the backend
 * plugin, initializes the GPU, and provides factory methods for renderers
 * and surfaces. The app typically creates one context at startup.
 */
class IRenderContext : public velk::Interface<IRenderContext>
{
public:
    /** @brief Initializes the context: loads the backend plugin and sets up the GPU. */
    virtual bool init(const RenderConfig& config) = 0;

    /** @brief Creates a render target surface with the given dimensions. */
    virtual ISurface::Ptr create_surface(int width, int height) = 0;

    /** @brief Creates a renderer backed by this context's backend. */
    virtual IRenderer::Ptr create_renderer() = 0;

    /**
     * @brief Creates a shader material from GLSL source.
     *
     * Compiles the shader, registers the pipeline with the backend,
     * and returns a material with the pipeline handle set.
     * If vertex_source is nullptr, a default instanced quad vertex shader is used.
     * Returns nullptr on compilation failure.
     */
    virtual velk::IObject::Ptr create_shader_material(const char* fragment_source,
                                                       const char* vertex_source = nullptr) = 0;
};

} // namespace velk_ui

#endif // VELK_UI_INTF_RENDER_CONTEXT_H
