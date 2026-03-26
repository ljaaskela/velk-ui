#ifndef VELK_UI_SURFACE_IMPL_H
#define VELK_UI_SURFACE_IMPL_H

#include <velk/ext/object.h>

#include <velk-ui/interface/intf_surface.h>
#include <velk-ui/plugins/render/plugin.h>

namespace velk_ui {

class Surface : public velk::ext::Object<Surface, ISurface>
{
public:
    VELK_CLASS_UID(ClassId::Surface, "Surface");
};

} // namespace velk_ui

#endif // VELK_UI_SURFACE_IMPL_H
