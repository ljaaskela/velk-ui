#ifndef VELK_RENDER_INTF_RENDER_CONTEXT_H
#define VELK_RENDER_INTF_RENDER_CONTEXT_H

#include <velk/interface/intf_metadata.h>

#include <unordered_map>
#include <velk-render/interface/intf_render_backend.h>
#include <velk-render/interface/intf_surface.h>
#include <velk-render/render_types.h>

namespace velk {

/**
 * @brief Owns the render backend and provides rendering infrastructure.
 *
 * The context is created via create_render_context(). It loads the backend
 * plugin, initializes the GPU, and provides factory methods for surfaces
 * and shader materials.
 */
class IRenderContext : public Interface<IRenderContext>
{
public:
    virtual bool init(const RenderConfig& config) = 0;
    virtual ISurface::Ptr create_surface(int width, int height) = 0;

    /**
     * @brief Creates a shader material from GLSL source.
     *
     * Compiles the shader, registers the pipeline with the backend,
     * and returns a material with the pipeline handle set.
     * If vertex_source is nullptr, a default instanced quad vertex shader is used.
     * Returns nullptr on compilation failure.
     */
    virtual IObject::Ptr create_shader_material(const char* fragment_source,
                                                const char* vertex_source = nullptr) = 0;

    /**
     * @brief Compiles GLSL shaders and registers the pipeline.
     *
     * Returns a pipeline key that can be stored by materials that manage
     * their own GPU data (e.g. via ext::Material). Returns 0 on failure.
     */
    /// Compiles GLSL shaders, registers the pipeline, and returns the key.
    /// If key is 0, a new key is auto-assigned. Otherwise the given key is used.
    virtual uint64_t compile_pipeline(const char* fragment_source, const char* vertex_source,
                                      uint64_t key = 0) = 0;

    /// Returns the mapping from pipeline keys to backend PipelineId handles.
    virtual const std::unordered_map<uint64_t, PipelineId>& pipeline_map() const = 0;

    /// Registers a virtual shader include file.
    /// Shaders can then use #include "name" to pull in the content.
    virtual void register_shader_include(string_view name, string_view content) = 0;

    /// Returns the render backend.
    virtual IRenderBackend::Ptr backend() const = 0;
};

} // namespace velk

#endif // VELK_RENDER_INTF_RENDER_CONTEXT_H
