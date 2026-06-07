#pragma once

#include <cstdint>
#include <ostream>

class ContentManager;

class SelfPlay
{
public:
    static int run(const ContentManager& content, std::uint32_t baseSeed, int games, std::ostream& out);
};

