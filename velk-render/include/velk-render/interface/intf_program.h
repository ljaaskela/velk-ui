#ifndef VELK_RENDER_INTF_PROGRAM_H
#define VELK_RENDER_INTF_PROGRAM_H

#include <velk/interface/intf_metadata.h>

#include <velk/string_view.h>
#include <velk-render/interface/intf_gpu_resource.h>

#include <cstddef>
#include <cstdint>

namespace velk {

class IRenderContext;

/**
 * @brief A GPU shader program: a pipeline handle plus per-draw GPU data.
 *
 * The renderer binds the pipeline returned by `get_pipeline_handle()` and
 * writes `gpu_data_size()` bytes of per-draw state via `write_gpu_data()`
 * immediately after the `DrawDataHeader` in the staging buffer. The shader
 * reads this data via `buffer_reference` from the draw data pointer.
 *
 * The pipeline handle is a GPU resource and participates in the
 * `IGpuResource` observer protocol for frame-deferred destruction: when the
 * program is destroyed, the renderer queues the pipeline for destruction
 * after the frames currently in flight have finished.
 *
 * Chain: IInterface -> IGpuResource -> IProgram
 */
class IProgram : public Interface<IProgram, IGpuResource>
{
public:
    GpuResourceType get_type() const override { return GpuResourceType::Program; }

    /** @brief Returns the pipeline handle, compiling lazily if needed. */
    virtual uint64_t get_pipeline_handle(IRenderContext& ctx) = 0;

    /**
     * @brief Stores the compiled pipeline handle.
     *
     * Called by the render context after pipeline compilation. Implementations
     * cache the handle for subsequent `get_pipeline_handle()` calls.
     */
    virtual void set_pipeline_handle(uint64_t handle) = 0;

    /** @brief Returns the size in bytes of this program's per-draw GPU data. */
    virtual size_t gpu_data_size() const = 0;

    /**
     * @brief Writes per-draw GPU data into the staging buffer.
     * @param out  Destination buffer (immediately after DrawDataHeader).
     * @param size Buffer size in bytes (equals gpu_data_size()).
     * @return ReturnValue::Success on success, ReturnValue::Fail on error.
     */
    virtual ReturnValue write_gpu_data(void* out, size_t size) const = 0;

    /**
     * @brief Returns a GLSL snippet that defines this program's fill function
     *        for composed shaders (e.g. the RT compute tracer).
     *
     * Convention: the snippet is self-contained and declares
     *   - Any buffer_reference structs it needs for its per-draw data
     *     (matching what write_gpu_data emits).
     *   - Exactly one function with the signature
     *        vec4 <fn_name>(uint64_t data_addr, vec2 uv)
     *     where data_addr points to the bytes produced by write_gpu_data and
     *     uv is the shape-local texture coordinate in [0, 1].
     * The concrete function name is returned by get_fill_fn_name() so the
     * composer can generate a dispatch switch.
     *
     * Default empty = "no snippet"; the consumer falls back to a solid base
     * color. Rasterization is unchanged either way.
     */
    virtual string_view get_fill_src() const { return {}; }

    /**
     * @brief Returns the name of the fill function declared by get_fill_src().
     *
     * Must be unique across material classes (typically derived from the
     * class name, e.g. "velk_fill_gradient"). Empty when get_fill_src() is
     * empty.
     */
    virtual string_view get_fill_fn_name() const { return {}; }

    /**
     * @brief Returns the filename under which this material's fill snippet
     *        should be registered as a shader include (e.g. "velk_gradient.glsl").
     *
     * The renderer calls IRenderContext::register_shader_include(name, src)
     * with this name and the get_fill_src() body, then composed shaders
     * reference the snippet via `#include "<name>"`. Matches the pattern
     * the text plugin uses for velk_text.glsl. Empty when there is no
     * snippet to register.
     */
    virtual string_view get_fill_include_name() const { return {}; }

    /**
     * @brief Hook for materials whose fill snippet depends on additional
     *        shader includes (e.g. the text material's velk_text.glsl).
     *
     * The renderer calls this once when the material class is first
     * encountered on the RT path, before composing any compute pipeline
     * that references it. Implementations call ctx.register_shader_include()
     * for each dependency. Idempotent.
     */
    virtual void register_fill_includes(IRenderContext& /*ctx*/) const {}
};

} // namespace velk

#endif // VELK_RENDER_INTF_PROGRAM_H
