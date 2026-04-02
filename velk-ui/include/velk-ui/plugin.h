#ifndef VELK_UI_PLUGIN_H
#define VELK_UI_PLUGIN_H

#include <velk/common.h>

namespace velk_ui {

namespace ClassId {

/** @brief UI element node. Holds position, size, transform, and z-index. */
inline constexpr velk::Uid Element{"136ea22f-189a-4750-ad12-d4d15bd6b7cf"};

/** @brief Scene graph. Owns the element hierarchy, runs layout, and pushes changes to the renderer. */
inline constexpr velk::Uid Scene{"c9f5e3a4-0b6d-4f8c-ae7f-3d4e5a6b7c8d"};

namespace Constraint {

/** @brief Lays out children along a single axis with optional spacing. */
inline constexpr velk::Uid Stack{"b8e4d2f3-9a5c-4e7b-8d6f-2c3e4a5b6d7e"};

/** @brief Clamps an element to a fixed width and/or height. */
inline constexpr velk::Uid FixedSize{"a7f3c1d2-8e4b-4f6a-9c5d-1b2e3f4a5b6c"};

} // namespace Constraint

namespace Transform {

/** @brief Decomposed transform: translate, rotate (Z), scale. */
inline constexpr velk::Uid Trs{"f1a2b3c4-d5e6-4f7a-8b9c-0d1e2f3a4b5c"};

/** @brief Raw 4x4 matrix transform. */
inline constexpr velk::Uid Matrix{"e2b3c4d5-f6a7-4e8b-9c0d-1e2f3a4b5c6d"};

} // namespace Transform

namespace Visual {

/** @brief Solid color rectangle filling the element bounds. */
inline constexpr velk::Uid Rect{"e3a7b1c2-d4f5-4e6a-8b9c-0d1e2f3a4b5c"};

/** @brief Rounded rectangle with SDF corners. */
inline constexpr velk::Uid RoundedRect{"7b2c4d5e-6f8a-4e1b-9c3d-0e5f6a7b8c9d"};

} // namespace Visual

namespace Material {

/** @brief Custom fragment shader material. */
inline constexpr velk::Uid Shader{"d1e2f3a4-b5c6-4d7e-8f9a-0b1c2d3e4f5a"};

/** @brief Built-in linear gradient material. */
inline constexpr velk::Uid Gradient{"f2a3b4c5-d6e7-4f8a-9b0c-1d2e3f4a5b6c"};

} // namespace Material

} // namespace ClassId

namespace PluginId {

inline constexpr velk::Uid VelkUiPlugin{"45c450a1-5f11-4869-8f72-3bafaeae0079"};

} // namespace PluginId

} // namespace velk_ui

#endif // VELK_UI_PLUGIN_H
