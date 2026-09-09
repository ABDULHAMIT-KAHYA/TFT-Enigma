#pragma once

#include <cstdint>
#include <ostream>

struct UnitId
{
    std::uint64_t value = 0;
};

inline bool operator==(UnitId a, UnitId b)
{
    return a.value == b.value;
}

inline bool operator!=(UnitId a, UnitId b)
{
    return !(a == b);
}

inline bool operator<(UnitId a, UnitId b)
{
    return a.value < b.value;
}

inline bool isValid(UnitId id)
{
    return id.value != 0;
}

inline std::ostream& operator<<(std::ostream& os, UnitId id)
{
    os << id.value;
    return os;
}
