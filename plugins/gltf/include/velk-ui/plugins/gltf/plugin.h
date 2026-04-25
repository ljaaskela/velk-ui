#ifndef VELK_UI_GLTF_PLUGIN_H
#define VELK_UI_GLTF_PLUGIN_H

#include <velk/common.h>

namespace velk::ui {

namespace ClassId {

/** @brief Loaded glTF 2.0 asset; implements IGltfAsset and IResource. */
inline constexpr Uid GltfAsset{"efa03397-0d6d-49f8-b7ac-3de2faed7d67"};

/** @brief Decoder turning raw glTF / glb bytes into GltfAsset objects. Registered as "gltf". */
inline constexpr Uid GltfDecoder{"70c43148-bb8e-464f-a3f5-b6383052817d"};

} // namespace ClassId

namespace PluginId {

inline constexpr Uid GltfPlugin{"7b6e72e7-ae50-4d90-b106-751f48937eae"};

} // namespace PluginId

} // namespace velk::ui

#endif // VELK_UI_GLTF_PLUGIN_H
