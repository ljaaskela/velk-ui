#ifndef VELK_UI_INTF_SURFACE_H
#define VELK_UI_INTF_SURFACE_H

#include <velk/interface/intf_metadata.h>

namespace velk_ui {

/**
 * @brief A render target with dimensions.
 *
 * Created by the renderer via create_surface(). Scenes are attached to
 * surfaces for rendering. Multiple surfaces can exist simultaneously,
 * each rendering a different scene (or the same scene).
 */
class ISurface : public velk::Interface<ISurface>
{
public:
    VELK_INTERFACE(
        (PROP, int, width, 0),
        (PROP, int, height, 0)
    )
};

} // namespace velk_ui

#endif // VELK_UI_INTF_SURFACE_H
