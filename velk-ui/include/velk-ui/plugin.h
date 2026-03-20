#ifndef VELK_UI_PLUGIN_H
#define VELK_UI_PLUGIN_H

#include <velk/common.h>

namespace velk_ui {

namespace ClassId {

inline constexpr velk::Uid Element{"136ea22f-189a-4750-ad12-d4d15bd6b7cf"};
inline constexpr velk::Uid Scene{"c9f5e3a4-0b6d-4f8c-ae7f-3d4e5a6b7c8d"};
inline constexpr velk::Uid Stack{"b8e4d2f3-9a5c-4e7b-8d6f-2c3e4a5b6d7e"};
inline constexpr velk::Uid FixedSize{"a7f3c1d2-8e4b-4f6a-9c5d-1b2e3f4a5b6c"};

} // namespace ClassId

namespace PluginId {

inline constexpr velk::Uid VelkUiPlugin{"45c450a1-5f11-4869-8f72-3bafaeae0079"};

} // namespace PluginId

} // namespace velk_ui

#endif // VELK_UI_PLUGIN_H
