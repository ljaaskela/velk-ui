#ifndef VELK_SCENE_PLUGIN_IMPL_H
#define VELK_SCENE_PLUGIN_IMPL_H

#include <velk/ext/plugin.h>

#include <velk-scene/plugin.h>

namespace velk::scene {

class ScenePlugin final : public ::velk::ext::Plugin<ScenePlugin>
{
public:
    VELK_PLUGIN_UID(::velk::scene::PluginId::ScenePlugin);
    VELK_PLUGIN_NAME("velk-scene");
    VELK_PLUGIN_VERSION(0, 1, 0);

    ::velk::ReturnValue initialize(::velk::IVelk& velk, ::velk::PluginConfig& config) override;
    ::velk::ReturnValue shutdown(::velk::IVelk& velk) override;
};

} // namespace velk::scene

VELK_PLUGIN(::velk::scene::ScenePlugin)

#endif // VELK_SCENE_PLUGIN_IMPL_H
