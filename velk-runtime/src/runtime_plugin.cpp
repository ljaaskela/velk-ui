#include "runtime_plugin.h"

#include "application.h"

namespace velk::impl {

ReturnValue RuntimePlugin::initialize(IVelk& velk, PluginConfig&)
{
    return ::velk::register_type<Application>(velk);
}

ReturnValue RuntimePlugin::shutdown(IVelk&)
{
    return ReturnValue::Success;
}

} // namespace velk::impl
