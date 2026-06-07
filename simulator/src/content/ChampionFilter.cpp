#include "content/ChampionFilter.hpp"
#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

static std::string toLower(const std::string& s)
{
    std::string out;
    out.reserve(s.size());
    for (char c : s)
    {
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return out;
}

static bool hasTagLower(const ChampionDefinition& c, std::string_view tagLower)
{
    for (const std::string& t : c.tags)
    {
        if (toLower(t) == tagLower)
        {
            return true;
        }
    }
    return false;
}

bool isPlayableChampion(const ChampionDefinition& c)
{
    if (!c.isPlayable)
    {
        return false;
    }

    if (c.cost < 1 || c.cost > 5)
    {
        return false;
    }

    if (c.name.empty())
    {
        return false;
    }

    if (c.hp <= 0 || c.ad <= 0 || c.range <= 0 || c.attackSpeed <= 0.0f)
    {
        return false;
    }

    if (c.abilityId.empty())
    {
        return false;
    }

    if (hasTagLower(c, "summon") || hasTagLower(c, "helper") || hasTagLower(c, "object"))
    {
        return false;
    }

    bool hasTrait = false;
    for (const std::string& t : c.traits)
    {
        if (!t.empty())
        {
            hasTrait = true;
            break;
        }
    }
    if (!hasTrait)
    {
        return false;
    }

    return true;
}
