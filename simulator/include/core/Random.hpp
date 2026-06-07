#pragma once

#include <cstdint>

class Random
{
public:
    explicit Random(std::uint32_t seed = 1u);

    void setSeed(std::uint32_t seed);
    std::uint32_t nextU32();
    int nextInt(int maxExclusive);
    float nextFloat01();

private:
    std::uint32_t state_;
};

