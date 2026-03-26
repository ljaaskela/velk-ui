#ifndef VELK_UI_INTF_RENDERER_H
#define VELK_UI_INTF_RENDERER_H

#include <velk/interface/intf_metadata.h>

#include <velk-ui/interface/intf_surface.h>
#include <velk-ui/types.h>

namespace velk_ui {

class IScene;

/**
 * @brief Backend-agnostic renderer interface.
 *
 * The renderer is passive: it performs no work unless the app calls render().
 * Scenes are attached to surfaces; during render() the renderer pulls
 * SceneState from each attached scene, rebuilds batches as needed, and
 * submits them to the backend.
 */
class IRenderer : public velk::Interface<IRenderer>
{
public:
    /** @brief Initializes the renderer and loads the requested backend plugin. */
    virtual bool init(const RenderConfig& config) = 0;

    /** @brief Creates a render target surface with the given dimensions. */
    virtual ISurface::Ptr create_surface(int width, int height) = 0;

    /** @brief Attaches a scene to a surface for rendering. */
    virtual void attach(const ISurface::Ptr& surface, const velk::IInterface::Ptr& scene) = 0;

    /** @brief Detaches a surface, stopping rendering of its attached scene. */
    virtual void detach(const ISurface::Ptr& surface) = 0;

    /** @brief Pulls state from attached scenes, rebuilds batches, and draws. */
    virtual void render() = 0;

    /** @brief Releases all GPU resources and unloads the backend. */
    virtual void shutdown() = 0;
};

} // namespace velk_ui

#endif // VELK_UI_INTF_RENDERER_H
