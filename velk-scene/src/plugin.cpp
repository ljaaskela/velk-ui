#include "plugin.h"

#include "camera.h"
#include "deferred_path.h"
#include "element.h"
#include "forward_path.h"
#include "frame_data_manager.h"
#include "frame_snippet_registry.h"
#include "gpu_resource_manager.h"
#include "renderer.h"
#include "rt_path.h"
#include "scene.h"
#include "trait/light.h"
#include "transform/look_at.h"
#include "visual/cube_visual.h"
#include "visual/mesh_visual.h"
#include "visual/sphere_visual.h"
#include "transform/matrix.h"
#include "transform/orbit.h"
#include "transform/trs.h"

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
    rv &= register_type<::velk::GpuResourceManager>(velk, alloc);
    rv &= register_type<::velk::FrameDataManager>(velk, alloc);
    rv &= register_type<::velk::FrameSnippetRegistry>(velk, alloc);

    // Render paths. Hive-allocated (cheap to instantiate per camera).
    rv &= register_type<::velk::ForwardPath>(velk);
    rv &= register_type<::velk::DeferredPath>(velk);
    rv &= register_type<::velk::RtPath>(velk);

    rv &= register_type<Trs>(velk);
    rv &= register_type<Matrix>(velk);
    rv &= register_type<LookAt>(velk);
    rv &= register_type<Orbit>(velk);
    rv &= register_type<impl::Camera>(velk);
    rv &= register_type<impl::Light>(velk);
    rv &= register_type<impl::CubeVisual>(velk);
    rv &= register_type<impl::SphereVisual>(velk);
    rv &= register_type<impl::MeshVisual>(velk);
    return rv;
}

::velk::ReturnValue ScenePlugin::shutdown(::velk::IVelk& /*velk*/)
{
    return ::velk::ReturnValue::Success;
}

} // namespace velk
