#ifndef VELK_UI_GLTF_PLUGIN_IMPL_H
#define VELK_UI_GLTF_PLUGIN_IMPL_H

#include <velk/ext/plugin.h>

#include <velk-ui/plugins/gltf/plugin.h>

namespace velk::ui::impl {

class GltfPlugin final : public ::velk::ext::Plugin<GltfPlugin>
{
public:
    VELK_PLUGIN_UID(::velk::ui::PluginId::GltfPlugin);
    VELK_PLUGIN_NAME("velk-gltf");
    VELK_PLUGIN_VERSION(0, 1, 0);

    ReturnValue initialize(IVelk& velk, PluginConfig& config) override;
    ReturnValue shutdown(IVelk& velk) override;

private:
    IResourceDecoder::Ptr gltf_decoder_;
};

} // namespace velk::ui::impl

VELK_PLUGIN(velk::ui::impl::GltfPlugin)

#endif // VELK_UI_GLTF_PLUGIN_IMPL_H
