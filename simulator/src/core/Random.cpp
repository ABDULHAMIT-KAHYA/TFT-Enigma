#include "core/Random.hpp"
#include <algorithm>

Random::Random(std::uint32_t seed)
    : state_(seed == 0u ? 1u : seed)
{
}

void Random::setSeed(std::uint32_t seed)
{
    state_ = seed == 0u ? 1u : seed;
}

std::uint32_t Random::nextU32()
{
    std::uint32_t x = state_;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    state_ = x == 0u ? 1u : x;
    return state_;
}

int Random::nextInt(int maxExclusive)
{
    if (maxExclusive <= 1)
    {
        return 0;
    }
    return static_cast<int>(nextU32() % static_cast<std::uint32_t>(maxExclusive));
}

float Random::nextFloat01()
{
    const std::uint32_t x = nextU32();
    const float f = static_cast<float>(x) / static_cast<float>(0xFFFFFFFFu);
    return std::clamp(f, 0.0f, 1.0f);
}

