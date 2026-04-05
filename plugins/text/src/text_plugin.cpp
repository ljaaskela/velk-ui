#include "text_plugin.h"

#include "font.h"
#include "visual/text_visual.h"

namespace velk::ui {

ReturnValue TextPlugin::initialize(IVelk& velk, PluginConfig&)
{
    auto rv = register_type<Font>(velk);
    rv &= register_type<TextVisual>(velk);

    // Create shared default font
    auto obj = ::velk::instance().create<IObject>(ClassId::Font);
    default_font_ = interface_pointer_cast<IFont>(obj);
    if (default_font_) {
        default_font_->init_default();
        default_font_->set_size(16.f);
    }

    return rv;
}

ReturnValue TextPlugin::shutdown(IVelk&)
{
    default_font_ = nullptr;
    return ReturnValue::Success;
}

IFont::Ptr TextPlugin::default_font() const
{
    return default_font_;
}

} // namespace velk::ui
