#ifndef VELK_RENDER_PLUGINS_RT_PLUGIN_H
#define VELK_RENDER_PLUGINS_RT_PLUGIN_H

// velk_rt sub-plugin: registers RtPath (compute-shader path tracer).
//
// The plugin must be loaded after velk_render (it registers a render
// path that consumes velk-render-side types). Public ID lives in
// velk-render/plugin.h as PluginId::RtPlugin so user code can check
// for its presence with `plugin_registry().is_loaded(PluginId::RtPlugin)`
// before calling `path::create_rt()`.
//
// Currently implements the SW (compute-shader) path tracer for any
// IRenderBackend; future revisions will detect HW RT support via a
// new `IRenderBackend::supports_hw_ray_tracing()` cap query and route
// to a HW backing transparently.

#include <velk-render/plugin.h>

#endif // VELK_RENDER_PLUGINS_RT_PLUGIN_H
