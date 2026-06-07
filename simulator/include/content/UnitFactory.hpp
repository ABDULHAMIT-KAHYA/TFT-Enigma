#pragma once

#include <string_view>
#include "content/Ability.hpp"
#include "content/ContentManager.hpp"
#include "core/Position.hpp"
#include "core/TeamId.hpp"
#include "core/Unit.hpp"
class UnitFactory
{
public:
    explicit UnitFactory(const ContentManager& content);

    Unit createFromChampion(std::string_view championName,
                            const Position& position,
                            TeamId team) const;

private:
    const ContentManager* content_;
};

