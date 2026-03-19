#ifndef VELK_UI_INTF_ELEMENT_H
#define VELK_UI_INTF_ELEMENT_H

#include <velk/api/math_types.h>
#include <velk/interface/intf_metadata.h>

namespace velk_ui {

class IElement : public velk::Interface<IElement>
{
public:
    VELK_INTERFACE(
        (PROP, velk::vec3, position, {}),
        (PROP, velk::size, size, {}),
        (PROP, velk::color, color, {}),
        (PROP, velk::mat4, local_transform, {}),
        (RPROP, velk::mat4, world_matrix, {}),
        (PROP, int32_t, z_index, 0)
    )
};

} // namespace velk_ui

#endif // VELK_UI_INTF_ELEMENT_H
