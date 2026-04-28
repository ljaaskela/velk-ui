#ifndef VELK_RENDER_EXT_RENDER_PATH_H
#define VELK_RENDER_EXT_RENDER_PATH_H

#include <velk/ext/core_object.h>

#include <velk-render/interface/intf_render_path.h>

namespace velk::ext {

/**
 * @brief CRTP base providing empty default impls for `IRenderPath` hooks.
 *
 * Concrete render paths inherit from this instead of `ObjectCore`
 * directly. `build_passes()` stays pure — every path must implement it.
 * Other hooks (`needs`, `on_view_removed`, `shutdown`,
 * `find_gbuffer_group`, `find_shadow_debug_tex`) get empty defaults
 * suitable for paths with no per-view state or no G-buffer.
 */
template <class FinalClass, class... ExtraInterfaces>
class RenderPath
    : public ObjectCore<FinalClass, ::velk::IRenderPath, ExtraInterfaces...>
{
public:
    ::velk::IRenderPath::Needs needs() const override { return {}; }

    void on_view_removed(::velk::ViewEntry& /*view*/, ::velk::FrameContext& /*ctx*/) override {}

    void shutdown(::velk::FrameContext& /*ctx*/) override {}

    ::velk::RenderTargetGroup find_gbuffer_group(::velk::ViewEntry* /*view*/) const override
    {
        return 0;
    }

    ::velk::TextureId find_shadow_debug_tex(::velk::ViewEntry* /*view*/) const override
    {
        return 0;
    }
};

} // namespace velk::ext

#endif // VELK_RENDER_EXT_RENDER_PATH_H
