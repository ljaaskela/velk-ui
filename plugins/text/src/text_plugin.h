#ifndef VELK_UI_TEXT_PLUGIN_IMPL_H
#define VELK_UI_TEXT_PLUGIN_IMPL_H

#include <velk/ext/plugin.h>

#include <velk-ui/plugins/text/plugin.h>

namespace velk_ui {

class TextPlugin final : public velk::ext::Plugin<TextPlugin>
{
public:
    VELK_PLUGIN_UID(PluginId::TextPlugin);
    VELK_PLUGIN_NAME("velk_text");
    VELK_PLUGIN_VERSION(0, 1, 0);

    velk::ReturnValue initialize(velk::IVelk& velk, velk::PluginConfig& config) override;
    velk::ReturnValue shutdown(velk::IVelk& velk) override;
};

} // namespace velk_ui

VELK_PLUGIN(velk_ui::TextPlugin)

#endif // VELK_UI_TEXT_PLUGIN_IMPL_H
