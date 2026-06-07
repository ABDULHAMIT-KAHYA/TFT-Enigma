#pragma once

#include <cstdint>
#include <ostream>
#include <string>

class TFTDataImporter
{
public:
    struct ImportResult
    {
        int detectedSet = 0;
        std::int32_t champions = 0;
        std::int32_t traits = 0;
        std::int32_t items = 0;
        std::int32_t abilities = 0;
        std::int32_t warnings = 0;
    };

    ImportResult importLiveTft(const std::string& outputDataRoot, std::ostream& out);

private:
    bool downloadToFile(const std::string& url, const std::string& outFilePath, std::ostream& out);
};

