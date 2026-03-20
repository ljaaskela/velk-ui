#ifndef VELK_UI_TYPES_H
#define VELK_UI_TYPES_H

#include <velk/api/math_types.h>

#include <cstdint>

namespace velk_ui {

enum class DirtyFlags : uint8_t
{
    None    = 0,
    Layout  = 1,
    Visual  = 2,
    ZOrder  = 4
};

inline constexpr DirtyFlags operator|(DirtyFlags a, DirtyFlags b)
{
    return static_cast<DirtyFlags>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

inline constexpr DirtyFlags operator&(DirtyFlags a, DirtyFlags b)
{
    return static_cast<DirtyFlags>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
}

inline constexpr DirtyFlags& operator|=(DirtyFlags& a, DirtyFlags b)
{
    a = a | b;
    return a;
}

inline constexpr DirtyFlags& operator&=(DirtyFlags& a, DirtyFlags b)
{
    a = a & b;
    return a;
}

inline constexpr DirtyFlags operator~(DirtyFlags a)
{
    return static_cast<DirtyFlags>(~static_cast<uint8_t>(a));
}

enum class DimUnit : uint8_t
{
    None,
    Px,
    Pct
};

struct dim
{
    float value{};
    DimUnit unit{DimUnit::None};

    static constexpr dim none() { return {0.f, DimUnit::None}; }
    static constexpr dim fill() { return {100.f, DimUnit::Pct}; }
    static constexpr dim zero() { return {0.f, DimUnit::Px}; }
    static constexpr dim px(float v) { return {v, DimUnit::Px}; }
    static constexpr dim pct(float v) { return {v, DimUnit::Pct}; }

    constexpr bool operator==(const dim& rhs) const { return value == rhs.value && unit == rhs.unit; }
    constexpr bool operator!=(const dim& rhs) const { return !(*this == rhs); }
};

struct Constraint
{
    velk::aabb bounds{};
};

inline float resolve_dim(dim d, float available)
{
    switch (d.unit) {
    case DimUnit::Px:  return d.value;
    case DimUnit::Pct: return d.value * available;
    default:           return available;
    }
}

} // namespace velk_ui

#endif // VELK_UI_TYPES_H
