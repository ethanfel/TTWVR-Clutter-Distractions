// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <algorithm>
#include <charconv>
#include <cmath>
#include <string_view>

namespace trap {
// The inventory forms store condition in 5-point tiers. Normal wear is two tiers.
inline int snapWearTiers(std::string_view value) {
    constexpr int normalWear = 2;
    auto first = value.find_first_not_of(" \t");
    if (first == value.npos) return normalWear;
    value = value.substr(first, value.find_last_not_of(" \t") - first + 1);
    double factor = 0;
    const char* end = value.data() + value.size();
    auto parsed = std::from_chars(value.data(), end, factor);
    if (parsed.ec != std::errc{} || parsed.ptr != end || !std::isfinite(factor)) return normalWear;
    return static_cast<int>(std::lround(std::clamp(factor, 0.0, 10.0) * normalWear));
}
}
