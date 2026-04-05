#include "render_plugin.h"

#include "render_context.h"
#include "shader.h"
#include "shader_material.h"
#include "surface.h"

namespace velk {

ReturnValue RenderPlugin::initialize(IVelk& velk, PluginConfig& config)
{
    auto rv = register_type<RenderContextImpl>(velk);
    rv &= register_type<Shader>(velk);
    rv &= register_type<Surface>(velk);
    rv &= register_type<impl::ShaderMaterial>(velk);
    return rv;
}

ReturnValue RenderPlugin::shutdown(IVelk&)
{
    return ReturnValue::Success;
}

} // namespace velk
