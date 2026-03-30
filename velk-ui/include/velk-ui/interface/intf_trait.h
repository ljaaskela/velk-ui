#ifndef VELK_UI_INTF_TRAIT_H
#define VELK_UI_INTF_TRAIT_H

#include <velk/interface/intf_interface.h>

#include <cstdint>

namespace velk_ui {

/**
 * @brief The four phases of the element pipeline.
 *
 * 1. Layout: walks children, divides space (e.g. Stack)
 * 2. Constraint: touches self only, refines size (e.g. FixedSize)
 * 3. Transform: modifies the world matrix after layout (e.g. Trs)
 * 4. Visual: produces draw commands for rendering (e.g. RectVisual)
 */
enum class TraitPhase : uint8_t
{
    Layout,     ///< Runs first. May read/write children via hierarchy.
    Constraint, ///< Runs second. Touches only the element itself.
    Transform,  ///< Runs third. Modifies the world matrix.
    Visual      ///< Runs last. Produces draw commands for rendering.
};

/**
 * @brief Base interface for UI traits attachable to elements.
 *
 * All element-attachable behaviors (layout, constraints, transforms, visuals)
 * inherit this interface. Enables uniform discovery and management via
 * Element::add_trait / remove_trait.
 */
class ITrait : public velk::Interface<ITrait>
{
public:
    /** @brief Returns which phase of the element pipeline this trait belongs to. */
    virtual TraitPhase get_phase() const = 0;
};

} // namespace velk_ui

#endif // VELK_UI_INTF_TRAIT_H
