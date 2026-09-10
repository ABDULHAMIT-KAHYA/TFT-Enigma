#pragma once

#include <cstdint>
#include <iosfwd>
#include <string>

class ContentManager;

class AgentEvaluator
{
public:
    static int run(const ContentManager& content,
                   int games,
                   std::uint32_t seed,
                   const std::string& policyModelPath,
                   std::ostream& out);
};
