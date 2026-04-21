#ifndef VELK_UI_INTF_MESH_VISUAL_H
#define VELK_UI_INTF_MESH_VISUAL_H

#include <velk/interface/intf_metadata.h>

#include <velk-render/interface/intf_mesh.h>
#include <velk-render/interface/intf_render_context.h>

namespace velk::ui {

/**
 * @brief Side-interface declaring "this visual draws a 3D mesh".
 *
 * Independent of IVisual; concrete classes implement both. The
 * renderer calls `get_mesh(ctx)` during rebuild_commands and assigns
 * the returned mesh to every DrawEntry the visual produces. The
 * render context is passed so procedural primitives can reach the
 * `IMeshBuilder` to look up (or lazily build) their cached geometry.
 *
 * Mesh ownership is subclass-defined: procedural primitives (cube,
 * sphere) pull cached meshes from the builder keyed by their
 * subdivisions; a generic MeshVisual (future) would return a mesh
 * held in an ObjectRef PROP.
 */
class IMeshVisual : public Interface<IMeshVisual>
{
public:
    /// Returns the mesh the visual currently wants to draw. May be
    /// null; the renderer then falls back to its own defaults (unit
    /// quad) for the visual's DrawEntries.
    virtual ::velk::IMesh::Ptr get_mesh(::velk::IRenderContext& ctx) const = 0;
};

} // namespace velk::ui

#endif // VELK_UI_INTF_MESH_VISUAL_H
