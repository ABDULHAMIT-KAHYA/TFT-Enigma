#pragma once
// Position.hpp
#include <cstdint>
#include <cstdlib>
#include <algorithm>

struct Position
{
    std::int32_t x;
    std::int32_t y;
};

inline bool operator==(const Position& a, const Position& b)
{
    return a.x == b.x && a.y == b.y;
}

inline bool operator!=(const Position& a, const Position& b)
{
    return !(a == b);
}

inline std::int32_t distanceBetween(const Position& a, const Position& b)
{
    const std::int32_t dq = a.x - b.x;
    const std::int32_t dr = a.y - b.y;
    const std::int32_t ds = (-a.x - a.y) - (-b.x - b.y);

    return std::max({
        std::abs(dq),
        std::abs(dr),
        std::abs(ds)
    });
}
