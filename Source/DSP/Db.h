#pragma once

#include <algorithm>
#include <cmath>
#include <cstring>
#include <type_traits>

namespace pontedsp::mc2000::dsp {

template <typename T>
inline bool exactlyEqual(const T lhs, const T rhs) noexcept
{
    static_assert(std::is_floating_point_v<T>);
    return std::memcmp(&lhs, &rhs, sizeof(T)) == 0;
}

template <typename T>
inline bool exactlyZero(const T value) noexcept
{
    return exactlyEqual(value, T {});
}

inline double decibelsToGain(const double db) noexcept
{
    return std::pow(10.0, db * 0.05);
}

inline double gainToDecibels(const double gain, const double floorDb = -160.0) noexcept
{
    return gain > 0.0 ? std::max(floorDb, 20.0 * std::log10(gain)) : floorDb;
}

inline double clampFinite(const double value, const double minimum,
                          const double maximum, const double fallback) noexcept
{
    return std::isfinite(value) ? std::clamp(value, minimum, maximum) : fallback;
}

} // namespace pontedsp::mc2000::dsp
