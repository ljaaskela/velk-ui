#ifndef VELK_SCENE_PIPELINE_OPTIONS_HELPERS_H
#define VELK_SCENE_PIPELINE_OPTIONS_HELPERS_H

#include <velk/api/state.h>
#include <velk/interface/intf_object_storage.h>

#include <velk-render/interface/intf_mesh.h>
#include <velk-render/interface/intf_render_backend.h>
#include <velk-render/interface/material/intf_material_options.h>

namespace velk {

inline PipelineOptions pipeline_options_from_storage(IObjectStorage* storage)
{
    PipelineOptions o{};
    if (!storage) return o;
    auto opts = storage->template find_attachment<IMaterialOptions>();
    if (!opts) return o;
    auto r = read_state<IMaterialOptions>(opts.get());
    if (!r) return o;
    o.cull_mode = r->cull_mode;
    o.front_face = r->front_face;
    o.blend_mode = (r->alpha_mode == AlphaMode::Blend)
                       ? BlendMode::Alpha
                       : BlendMode::Opaque;
    o.depth_test = r->depth_test;
    o.depth_write = r->depth_write;
    return o;
}

inline Topology to_backend_topology(MeshTopology mt)
{
    return mt == MeshTopology::TriangleStrip
             ? Topology::TriangleStrip
             : Topology::TriangleList;
}

} // namespace velk

#endif // VELK_SCENE_PIPELINE_OPTIONS_HELPERS_H
