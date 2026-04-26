#include "plugin.h"

#include "element.h"
#include "renderer.h"
#include "scene.h"

namespace velk {

::velk::ReturnValue ScenePlugin::initialize(::velk::IVelk& velk,
                                            ::velk::PluginConfig& /*config*/)
{
    auto rv = register_type<::velk::impl::Element>(velk);
    rv &= register_type<::velk::impl::Scene>(velk);

    // Renderer: low instance count (one per app), alloc-on-demand.
    ::velk::TypeOptions alloc;
    alloc.policy = ::velk::CreationPolicy::Alloc;
    rv &= register_type<::velk::Renderer>(velk, alloc);
    return rv;
}

::velk::ReturnValue ScenePlugin::shutdown(::velk::IVelk& /*velk*/)
{
    return ::velk::ReturnValue::Success;
}

} // namespace velk
