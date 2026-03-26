#include "gl_plugin.h"

#include "gl_backend.h"

namespace velk_ui {

velk::ReturnValue GlPlugin::initialize(velk::IVelk& velk, velk::PluginConfig&)
{
    return velk::register_type<GlBackend>(velk);
}

velk::ReturnValue GlPlugin::shutdown(velk::IVelk&)
{
    return velk::ReturnValue::Success;
}

} // namespace velk_ui
