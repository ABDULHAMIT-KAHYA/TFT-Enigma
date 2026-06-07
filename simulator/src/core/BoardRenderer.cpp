#include "core/BoardRenderer.hpp"
#include "combat/StatSystem.hpp"
#include <iostream>
#include <vector>
#include <algorithm>

static bool containsPosition(const std::vector<Position>& positions, Position p)
{
    for (const Position& existing : positions)
    {
        if (existing.x == p.x && existing.y == p.y)
        {
            return true;
        }
    }
    return false;
}
static char symbolForUnit(const Unit& unit)
{
    const std::string& name = unit.getName();
    for (char c : name)
    {
        if (c >= 'A' && c <= 'Z') return c;
        if (c >= 'a' && c <= 'z') return static_cast<char>(c - 'a' + 'A');
    }
    return '?';
}

void BoardRenderer::print(const Board& board,
                          const std::vector<Unit>& units,
                          const std::vector<Position>& highlightedCells)
{
std::vector<std::vector<std::string>> grid(
    board.getHeight(),
    std::vector<std::string>(board.getWidth(), " . ")
);

    for (const Position& p : highlightedCells)
    {
        if (!board.isInside(p))
        {
            continue;
        }
        grid[p.y][p.x] = " # ";
    }

    for (const Unit& unit : units)
    {
        if (!unit.isAlive())
        {
            continue;
        }

        Position pos = unit.getPosition();

        if (!board.isInside(pos))
        {
            continue;
        }

const std::int32_t effectiveMaxHp =
    std::max<std::int32_t>(1, StatSystem::getFinalStatInt(unit, StatType::MaxHp));
const std::int32_t hpBucket =
    std::max<std::int32_t>(
        1,
        (unit.getHp() * 10) / effectiveMaxHp
    );

std::string cell;
cell += symbolForUnit(unit);
cell += std::to_string(hpBucket);
if (containsPosition(highlightedCells, pos))
{
    cell += "#";
}
else
{
    bool stunned = false;
    bool buffed = false;
    bool debuffed = false;

    for (const StatusEffect& effect : unit.statusEffects())
    {
        if (effect.remainingMs <= 0)
        {
            continue;
        }

        if (effect.effectType == StatusEffectType::CrowdControl
            && effect.crowdControlType == CrowdControlType::Stun)
        {
            stunned = true;
        }
        else if (effect.effectType == StatusEffectType::Buff
            || effect.effectType == StatusEffectType::Shield
            || effect.effectType == StatusEffectType::HealOverTime
            || effect.effectType == StatusEffectType::DamageReduction
            || effect.effectType == StatusEffectType::BonusAttackDamage
            || effect.effectType == StatusEffectType::BonusAbilityPower
            || effect.effectType == StatusEffectType::BonusAttackSpeed
            || effect.effectType == StatusEffectType::BonusArmor
            || effect.effectType == StatusEffectType::BonusMagicResist
            || effect.effectType == StatusEffectType::CritChanceBonus
            || effect.effectType == StatusEffectType::CritDamageBonus)
        {
            buffed = true;
        }
        else if (effect.effectType == StatusEffectType::Debuff
            || effect.effectType == StatusEffectType::DamageOverTime)
        {
            debuffed = true;
        }
    }

    char marker = ' ';
    if (stunned)
    {
        marker = 'S';
    }
    else if (unit.didCastThisTurn())
    {
        marker = '*';
    }
    else if (unit.didAttackThisTurn())
    {
        marker = '!';
    }
    else if (unit.didMoveThisTurn())
    {
        marker = '>';
    }
    else if (buffed)
    {
        marker = 'B';
    }
    else if (debuffed)
    {
        marker = 'D';
    }

    cell += marker;
}

grid[pos.y][pos.x] = cell;
    }

    std::cout << "\nBOARD\n";

   std::cout << "\n    ";

     for (std::int32_t x = 0; x < board.getWidth(); ++x)
     {
    std::cout << x << "   ";
    }

    std::cout << "\n";


for (std::int32_t y = 0; y < board.getHeight(); ++y)
{
    if (y % 2 == 1)
    {
        std::cout << "   ";
    }

    for (std::int32_t x = 0; x < board.getWidth(); ++x)
    {
        std::cout << " / \\ ";
    }

    std::cout << '\n';

    if (y % 2 == 1)
    {
        std::cout << "   ";
    }

    for (std::int32_t x = 0; x < board.getWidth(); ++x)
    {
       std::cout << "|" << grid[y][x] << "|";
    }

    std::cout << '\n';
}

    std::cout << '\n';
}
