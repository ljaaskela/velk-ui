#include "rt_plugin.h"

#include "rt_path.h"

namespace velk::rt {

ReturnValue RtPlugin::initialize(IVelk& velk, PluginConfig&)
{
    // RtPath is hive-allocated; cheap to instantiate per camera that
    // attaches one. No CreationPolicy::Alloc override.
    return register_type<::velk::RtPath>(velk);
}

ReturnValue RtPlugin::shutdown(IVelk&)
{
    return ReturnValue::Success;
}

} // namespace velk::rt
