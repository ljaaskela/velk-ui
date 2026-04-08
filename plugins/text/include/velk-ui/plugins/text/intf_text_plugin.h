#ifndef VELK_UI_TEXT_INTF_TEXT_PLUGIN_H
#define VELK_UI_TEXT_INTF_TEXT_PLUGIN_H

#include <velk/interface/intf_metadata.h>

#include <velk-ui/interface/intf_font.h>

namespace velk::ui {

/**
 * @brief Public interface for the text plugin.
 *
 * Provides access to the shared default font. Accessible via
 * plugin_registry().get_or_load_plugin<ITextPlugin>(PluginId::TextPlugin).
 */
class ITextPlugin : public Interface<ITextPlugin>
{
public:
    /** @brief Returns the shared default font (Inter Regular, 16px). */
    virtual IFont::Ptr get_default_font() const = 0;
};

} // namespace velk::ui

#endif // VELK_UI_TEXT_INTF_TEXT_PLUGIN_H
