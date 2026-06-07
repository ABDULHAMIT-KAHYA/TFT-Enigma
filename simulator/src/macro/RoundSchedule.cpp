#include "macro/RoundSchedule.hpp"
#include <sstream>

RoundInfo RoundSchedule::get(int roundIndex)
{
    RoundInfo r{};

    if (roundIndex < 0)
    {
        roundIndex = 0;
    }

    if (roundIndex <= 2)
    {
        r.stage = 1;
        r.round = roundIndex + 1;
        r.isPve = true;
        r.isNeutral = false;
    }
    else
    {
        const int idx = roundIndex - 3;
        r.stage = 2 + (idx / 7);
        r.round = 1 + (idx % 7);
        r.isNeutral = (r.round == 7);
        r.isPve = r.isNeutral ? true : false;
    }

    std::ostringstream ss;
    ss << r.stage << "-" << r.round;
    r.label = ss.str();
    return r;
}
