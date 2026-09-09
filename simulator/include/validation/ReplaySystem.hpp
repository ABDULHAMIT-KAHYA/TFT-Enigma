#pragma once

#include <cstdint>
#include <iosfwd>
#include <string>
class ContentManager;

struct ReplayRunResult
{
    bool ok = false;
    std::string winner{};
    std::uint64_t finalHash = 0;
    std::size_t frameCount = 0;
};

namespace ReplaySystem
{
    ReplayRunResult recordScenario(const ContentManager& content,
                                   const std::string& scenarioPath,
                                   const std::string& outputPath,
                                   std::ostream& out);

    ReplayRunResult playReplay(const ContentManager& content,
                               const std::string& replayPath,
                               std::ostream& out,
                               bool verify);
}
