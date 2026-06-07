#pragma once

#include <string>

struct RoundInfo
{
    int stage = 1;
    int round = 1;
    bool isPve = true;
    bool isNeutral = false;
    std::string label{};
};

class RoundSchedule
{
public:
    static RoundInfo get(int roundIndex);
};
