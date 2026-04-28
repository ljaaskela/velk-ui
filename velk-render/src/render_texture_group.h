#ifndef VELK_RENDER_RENDER_TEXTURE_GROUP_H
#define VELK_RENDER_RENDER_TEXTURE_GROUP_H

#include <velk/vector.h>

#include <velk-render/ext/gpu_resource.h>
#include <velk-render/interface/intf_render_texture_group.h>
#include <velk-render/plugin.h>
#include <velk-render/render_types.h>

namespace velk::impl {

/**
 * @brief Concrete `IRenderTextureGroup` implementation.
 *
 * Key conventions:
 *   - `GpuResourceKey::Default` (0) = group bind handle (passed to
 *     `begin_pass`). Stored in `ext::GpuResource`'s primary handle slot.
 *   - keys 1..N = attachment 0..N-1 bindless `TextureId`s. Attachment
 *     access in path code goes through `attachment(idx)` which adds
 *     the +1 offset.
 */
class RenderTextureGroup
    : public ::velk::ext::GpuResource<RenderTextureGroup, ::velk::IRenderTextureGroup>
{
    using Base = ::velk::ext::GpuResource<RenderTextureGroup, ::velk::IRenderTextureGroup>;

public:
    VELK_CLASS_UID(::velk::ClassId::RenderTextureGroup, "RenderTextureGroup");

    GpuResourceType get_type() const override { return GpuResourceType::Texture; }

    uint64_t get_gpu_handle(uint64_t key) const override
    {
        if (key == ::velk::GpuResourceKey::Default) {
            return Base::get_gpu_handle(key);
        }
        size_t idx = static_cast<size_t>(key - 1);
        return idx < attachments_.size() ? attachments_[idx] : 0;
    }

    void set_gpu_handle(uint64_t key, uint64_t value) override
    {
        if (key == ::velk::GpuResourceKey::Default) {
            Base::set_gpu_handle(key, value);
            return;
        }
        size_t idx = static_cast<size_t>(key - 1);
        if (idx >= attachments_.size()) {
            attachments_.resize(idx + 1, 0);
        }
        attachments_[idx] = static_cast<::velk::TextureId>(value);
    }

    // ISurface
    uvec2 get_dimensions() const override { return size_; }
    PixelFormat format() const override { return format_; }

    DepthFormat get_depth_format() const override { return depth_format_; }
    void set_depth_format(DepthFormat df) override { depth_format_ = df; }

    void set_size(uint32_t w, uint32_t h) { size_ = {w, h}; }
    void set_format(PixelFormat fmt) { format_ = fmt; }

    // IRenderTextureGroup
    size_t attachment_count() const override { return attachments_.size(); }
    void clear_attachments() override { attachments_.clear(); }

private:
    ::velk::vector<::velk::TextureId> attachments_;
    uvec2 size_{};
    PixelFormat format_ = PixelFormat::RGBA8;
    DepthFormat depth_format_ = DepthFormat::None;
};

} // namespace velk::impl

#endif // VELK_RENDER_RENDER_TEXTURE_GROUP_H
