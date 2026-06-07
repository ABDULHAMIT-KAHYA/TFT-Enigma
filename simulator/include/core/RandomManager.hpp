#pragma once

#include <cstdint>
#include "core/Random.hpp"
class RandomManager
{
public:
    RandomManager();
    explicit RandomManager(std::uint32_t seed);

    void setSeed(std::uint32_t seed);
    std::uint32_t seed() const;

    float randomFloat01();
    int randomInt(int maxExclusive);

    Random& rng();

    static RandomManager& global();

private:
    std::uint32_t seed_;
    Random rng_;
};

