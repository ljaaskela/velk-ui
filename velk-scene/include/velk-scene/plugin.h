#ifndef VELK_SCENE_PLUGIN_H
#define VELK_SCENE_PLUGIN_H

#include <velk/common.h>

namespace velk::scene {

namespace ClassId {

/** @brief UI element node. Holds position, size, transform, and z-index. */
inline constexpr Uid Element{"074fde67-68e0-43fb-b52d-265665f301ee"};

/** @brief Scene-wide BVH attachment; implements IBvh. Installed by the renderer on first frame. */
inline constexpr Uid SceneBvh{"d9678826-947d-4f92-beee-5746340fe072"};

} // namespace ClassId

namespace PluginId {

inline constexpr Uid ScenePlugin{"81d179c5-7e55-4623-98e8-b458c30147f9"};

} // namespace PluginId

} // namespace velk::scene

#endif // VELK_SCENE_PLUGIN_H
