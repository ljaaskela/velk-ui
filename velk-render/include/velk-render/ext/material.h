#ifndef VELK_RENDER_EXT_MATERIAL_H
#define VELK_RENDER_EXT_MATERIAL_H

#include <velk/ext/object.h>

#include <velk-render/interface/intf_material_internal.h>
#include <velk-render/interface/intf_render_context.h>

namespace velk::ext {

/**
 * @brief CRTP base for application-defined materials.
 *
 * Use this when you write both the shader and the material class, and
 * control the GPU data layout directly. Provides pipeline handle storage
 * and lazy compilation via ensure_pipeline(). Derived classes implement
 * gpu_data_size() and write_gpu_data().
 *
 * For dynamic materials where inputs are discovered from the shader,
 * use ShaderMaterial instead.
 *
 * Usage:
 *   class MyMaterial : public velk::ext::Material<MyMaterial, IMyProps>
 *   {
 *       VELK_CLASS_UID(...);
 *       size_t gpu_data_size() const override { return sizeof(MyParams); }
 *       void write_gpu_data(void* out, size_t size) const override { ... }
 *   };
 */
template <class T, class... Extra>
class Material : public Object<T, IMaterialInternal, Extra...>
{
public:
    uint64_t get_pipeline_handle(IRenderContext&) override { return handle_; }
    void set_pipeline_handle(uint64_t handle) override { handle_ = handle; }

protected:
    /// Compile from source on first call, cache the pipeline handle.
    uint64_t ensure_pipeline(IRenderContext& ctx, string_view fragment_source, string_view vertex_source = {})
    {
        if (!has_pipeline_handle()) {
            handle_ = ctx.compile_pipeline(fragment_source, vertex_source);
        }
        return handle_;
    }

    /// Create from pre-compiled shaders. Nullptr uses the registered default.
    uint64_t ensure_pipeline(IRenderContext& ctx, const IShader::Ptr& fragment,
                             const IShader::Ptr& vertex = {})
    {
        if (!has_pipeline_handle()) {
            handle_ = ctx.create_pipeline(vertex, fragment);
        }
        return handle_;
    }

    /// Returns true if pipeline handle has already been created
    bool has_pipeline_handle() const { return handle_ != 0; }

    /// Writes a typed material params struct into the GPU data buffer.
    /// Zero-initializes the struct, then calls the provided function to fill it.
    template <typename Params, typename Fn>
    static ReturnValue set_material(void* out, size_t size, Fn&& fn)
    {
        if (size != sizeof(Params)) {
            return ReturnValue::Fail;
        }
        auto& p = *static_cast<Params*>(out);
        p = {};
        fn(p);
        return ReturnValue::Success;
    }

private:
    uint64_t handle_{};
};

} // namespace velk::ext

#endif // VELK_RENDER_EXT_MATERIAL_H
