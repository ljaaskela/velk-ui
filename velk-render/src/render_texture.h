#ifndef VELK_RENDER_RENDER_TEXTURE_H
#define VELK_RENDER_RENDER_TEXTURE_H

#include <velk-render/ext/gpu_resource.h>
#include <velk-render/interface/intf_render_target.h>
#include <velk-render/interface/intf_texture.h>
#include <velk-render/plugin.h>

namespace velk {

/**
 * @brief A texture that can be both rendered into and sampled.
 *
 * Implements IRenderTarget (for use as a begin_pass target) and ITexture
 * (for sampling in shaders via its bindless index). The backend lazily
 * creates GPU resources on first begin_pass.
 *
 * Created via instance().create<IRenderTarget>(ClassId::RenderTexture).
 * Set width, height, and format before first use.
 */
class RenderTexture : public ext::GpuResource<RenderTexture, IRenderTarget, ITexture>
{
public:
    VELK_CLASS_UID(ClassId::RenderTexture, "RenderTexture");

    GpuResourceType get_type() const override { return GpuResourceType::Texture; }

    // IRenderTarget
    uint64_t get_render_target_id() const override { return render_target_id_; }
    void set_render_target_id(uint64_t id) override { render_target_id_ = id; }

    // ITexture
    int width() const override { return width_; }
    int height() const override { return height_; }
    PixelFormat format() const override { return format_; }

    void set_size(int w, int h) { width_ = w; height_ = h; }
    void set_format(PixelFormat fmt) { format_ = fmt; }

    // IBuffer (no CPU-side data for render textures)
    size_t get_size() const override { return 0; }
    const uint8_t* get_data() const override { return nullptr; }
    bool is_dirty() const override { return false; }
    void clear_dirty() override {}

private:
    uint64_t render_target_id_ = 0;
    int width_ = 0;
    int height_ = 0;
    PixelFormat format_ = PixelFormat::RGBA8;
};

} // namespace velk

#endif // VELK_RENDER_RENDER_TEXTURE_H
