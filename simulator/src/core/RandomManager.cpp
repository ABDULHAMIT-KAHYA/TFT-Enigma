#include "core/RandomManager.hpp"
#include <algorithm>

RandomManager::RandomManager()
    : seed_(1u),
      rng_(1u)
{
}

RandomManager::RandomManager(std::uint32_t seed)
    : seed_(seed),
      rng_(seed)
{
}

void RandomManager::setSeed(std::uint32_t seed)
{
    seed_ = seed;
    rng_.setSeed(seed);
}

std::uint32_t RandomManager::seed() const
{
    return seed_;
}

std::uint32_t RandomManager::state() const
{
    return rng_.state();
}

void RandomManager::setState(std::uint32_t state)
{
    rng_.setState(state);
}
float RandomManager::randomFloat01()
{
    return rng_.nextFloat01();
}

int RandomManager::randomInt(int maxExclusive)
{
    return rng_.nextInt(maxExclusive);
}

Random& RandomManager::rng()
{
    return rng_;
}

RandomManager& RandomManager::global()
{
    static RandomManager g(1u);
    return g;
}


