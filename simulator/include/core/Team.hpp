#pragma once

#include <string>
#include <vector>
#include "core/Unit.hpp"
class Team
{
public:
    Team(std::string name, std::vector<Unit> units);

    const std::string& name() const;
    std::vector<Unit>& units();
    const std::vector<Unit>& units() const;
    bool hasAlive() const;

private:
    std::string name_;
    std::vector<Unit> units_;
};
