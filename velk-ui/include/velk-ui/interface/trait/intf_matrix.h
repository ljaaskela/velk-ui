#ifndef VELK_UI_INTF_MATRIX_H
#define VELK_UI_INTF_MATRIX_H

#include <velk/api/math_types.h>
#include <velk/interface/intf_metadata.h>

namespace velk_ui {

/**
 * @brief Raw 4x4 matrix transform.
 *
 * Multiplied into the world matrix after layout is finalized.
 */
class IMatrix : public velk::Interface<IMatrix>
{
public:
    VELK_INTERFACE(
        (PROP, velk::mat4, matrix, {})
    )
};

} // namespace velk_ui

#endif // VELK_UI_INTF_MATRIX_H
