#include "core/Team.hpp"
#include <utility>

Team::Team(std::string name, std::vector<Unit> units)
    : name_(std::move(name)),
      units_(std::move(units))
{
}

const std::string& Team::name() const
{
    return name_;
}

std::vector<Unit>& Team::units()
{
    return units_;
}

const std::vector<Unit>& Team::units() const
{
    return units_;
}

bool Team::hasAlive() const
{
    for (const Unit& unit : units_)
    {
        if (unit.isAlive())
        {
            return true;
        }
    }

    return false;
}
