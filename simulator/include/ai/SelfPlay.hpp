#pragma once

#include <cstdint>
#include <ostream>
#include <string>

class ContentManager;

class SelfPlay
{
public:
    static int run(const ContentManager& content,
                   std::uint32_t baseSeed,
                   int games,
                   const std::string& outputPath,
                   std::ostream& out);
};

