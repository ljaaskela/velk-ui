#ifndef VELK_RENDER_PLUGINS_RT_PLUGIN_IMPL_H
#define VELK_RENDER_PLUGINS_RT_PLUGIN_IMPL_H

#include <velk/ext/plugin.h>

#include <velk-render/plugin.h>
#include <velk-render/plugins/rt/plugin.h>

namespace velk::rt {

class RtPlugin final : public ext::Plugin<RtPlugin>
{
public:
    VELK_PLUGIN_UID(PluginId::RtPlugin);
    VELK_PLUGIN_NAME("velk-rt");
    VELK_PLUGIN_VERSION(0, 1, 0);

    ReturnValue initialize(IVelk& velk, PluginConfig& config) override;
    ReturnValue shutdown(IVelk& velk) override;
};

} // namespace velk::rt

VELK_PLUGIN(velk::rt::RtPlugin)

#endif // VELK_RENDER_PLUGINS_RT_PLUGIN_IMPL_H
