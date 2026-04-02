#ifndef VELK_UI_RENDER_PLUGIN_IMPL_H
#define VELK_UI_RENDER_PLUGIN_IMPL_H

#include <velk/ext/plugin.h>

#include <velk-ui/plugins/render/plugin.h>

namespace velk_ui {

class RenderPlugin final : public velk::ext::Plugin<RenderPlugin>
{
public:
    VELK_PLUGIN_UID(PluginId::RenderPlugin);
    VELK_PLUGIN_NAME("velk_render");
    VELK_PLUGIN_VERSION(0, 1, 0);

    velk::ReturnValue initialize(velk::IVelk& velk, velk::PluginConfig& config) override;
    velk::ReturnValue shutdown(velk::IVelk& velk) override;
};

} // namespace velk_ui

VELK_PLUGIN(velk_ui::RenderPlugin)

#endif // VELK_UI_RENDER_PLUGIN_IMPL_H
