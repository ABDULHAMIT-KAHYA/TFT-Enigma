#include "ai/AIPlayer.hpp"
#include "constants/AIConstants.hpp"
#include "constants/GameConstants.hpp"
#include "constants/MacroConstants.hpp"
#include <algorithm>
#include <cmath>
#include <ostream>
#include <sstream>

static std::vector<std::string> sortedKeys(const std::unordered_map<std::string, int>& m)
{
    std::vector<std::string> keys;
    keys.reserve(m.size());
    for (const auto& [k, _] : m)
    {
        keys.push_back(k);
    }
    std::sort(keys.begin(), keys.end());
    return keys;
}

float BoardScore::total() const
{
    return frontline * AIConstants::AIPlayerBoardScoreFrontlineWeight +
           dps * AIConstants::AIPlayerBoardScoreDpsWeight +
           survivability * AIConstants::AIPlayerBoardScoreSurvivabilityWeight +
           traitSynergy * AIConstants::AIPlayerBoardScoreTraitSynergyWeight +
           carryPotential * AIConstants::AIPlayerBoardScoreCarryPotentialWeight;
}

AIPlayer::AIPlayer(std::string name, std::uint32_t seed)
    : name_(std::move(name)),
      seed_(seed),
      targetTrait_()
{
}

std::uint32_t AIPlayer::mixSeed(std::uint32_t a, std::uint32_t b) const
{
    std::uint32_t x =
        a ^ (b + GameConstants::SeedMixConstant + (a << GameConstants::SeedMixShiftA) + (a >> GameConstants::SeedMixShiftB));
    x ^= (x << GameConstants::SeedScrambleShift1);
    x ^= (x >> GameConstants::SeedScrambleShift2);
    x ^= (x << GameConstants::SeedScrambleShift3);
    return x;
}

std::unordered_map<std::string, int> AIPlayer::traitCounts(const PlayerState& player,
                                                           const ContentManager& content) const
{
    std::unordered_map<std::string, int> counts;
    for (const OwnedUnit& u : player.board())
    {
        const ChampionDefinition* def = content.getChampion(u.championName);
        if (!def)
        {
            continue;
        }
        for (const std::string& t : def->traits)
        {
            counts[t] += 1;
        }
    }
    return counts;
}

float AIPlayer::unitDpsScore(const ChampionDefinition& def) const
{
    return static_cast<float>(def.ad) * def.attackSpeed * (1.0f + def.critChance * (def.critDamage - 1.0f));
}

float AIPlayer::unitTankScore(const ChampionDefinition& def) const
{
    const float ehpArmor = static_cast<float>(def.hp) * (1.0f + static_cast<float>(def.armor) / AIConstants::PercentScale);
    const float ehpMr =
        static_cast<float>(def.hp) * (1.0f + static_cast<float>(def.magicResist) / AIConstants::PercentScale);
    const float ehp = AIConstants::AIPlayerHalf * (ehpArmor + ehpMr);
    const float durMult = 1.0f / std::max(AIConstants::AIPlayerDurabilityMinDenominator, 1.0f - def.durability);
    return ehp * durMult;
}

float AIPlayer::unitCarryScore(const ChampionDefinition& def) const
{
    const float rangeBonus =
        static_cast<float>(std::clamp(def.range, 1, AIConstants::AIPlayerRangeClampMax)) * AIConstants::AIPlayerRangeBonusPerTile;
    const float dps = unitDpsScore(def);
    return dps * AIConstants::AIPlayerCarryDpsWeight + rangeBonus + def.abilityPower * AIConstants::AIPlayerCarryApWeight;
}

float AIPlayer::itemCarryWeight(const Item& item) const
{
    float score = 0.0f;
    for (const StatusEffect& s : item.passiveStats)
    {
        if (s.effectType == StatusEffectType::BonusAttackDamage) score += AIConstants::AIPlayerItemCarryWeightAttackDamage * s.value;
        if (s.effectType == StatusEffectType::BonusAttackSpeed) score += AIConstants::AIPlayerItemCarryWeightAttackSpeed * s.value;
        if (s.effectType == StatusEffectType::CritChanceBonus) score += AIConstants::AIPlayerItemCarryWeightCritChance * s.value;
        if (s.effectType == StatusEffectType::CritDamageBonus) score += AIConstants::AIPlayerItemCarryWeightCritDamage * s.value;
        if (s.effectType == StatusEffectType::BonusAbilityPower) score += AIConstants::AIPlayerItemCarryWeightAbilityPower * s.value;
    }
    return score;
}

float AIPlayer::itemTankWeight(const Item& item) const
{
    float score = 0.0f;
    for (const StatusEffect& s : item.passiveStats)
    {
        if (s.effectType == StatusEffectType::BonusArmor) score += AIConstants::AIPlayerItemTankWeightArmor * s.value;
        if (s.effectType == StatusEffectType::BonusMagicResist) score += AIConstants::AIPlayerItemTankWeightMagicResist * s.value;
        if (s.effectType == StatusEffectType::BonusAttackSpeed) score += AIConstants::AIPlayerItemTankWeightAttackSpeed * s.value;
        if (s.effectType == StatusEffectType::Buff && s.affectedStat == StatType::MaxHp) score += AIConstants::AIPlayerItemTankWeightMaxHp * s.value;
        if (s.effectType == StatusEffectType::DamageReduction) score += AIConstants::AIPlayerItemTankWeightDamageReduction * s.value;
    }
    return score;
}

BoardScore AIPlayer::evaluateBoard(const PlayerState& player,
                                  const ContentManager& content) const
{
    BoardScore s{};

    float totalDps = 0.0f;
    float totalTank = 0.0f;
    float totalCarry = 0.0f;

    for (const OwnedUnit& u : player.board())
    {
        const ChampionDefinition* def = content.getChampion(u.championName);
        if (!def)
        {
            continue;
        }

        float dps = unitDpsScore(*def);
        float tank = unitTankScore(*def);
        float carry = unitCarryScore(*def);

        for (const std::string& itemName : u.items)
        {
            if (const Item* item = content.getItem(itemName))
            {
                carry += itemCarryWeight(*item);
                tank += itemTankWeight(*item);
            }
        }

        totalDps += dps;
        totalTank += tank;
        totalCarry = std::max(totalCarry, carry);
    }

    s.dps = totalDps;
    s.survivability = totalTank;
    s.frontline = totalTank * AIConstants::AIPlayerFrontlineFromTankWeight;
    s.carryPotential = totalCarry;

    const auto counts = traitCounts(player, content);
    float synergy = 0.0f;
    for (const std::string& traitName : sortedKeys(counts))
    {
        const int c = counts.at(traitName);
        const TraitDefinition* def = content.getTrait(traitName);
        if (!def || def->tiers.empty())
        {
            synergy += static_cast<float>(c);
            continue;
        }

        int reached = 0;
        int next = def->tiers.front().breakpoint;
        for (const TraitTier& tier : def->tiers)
        {
            if (c >= tier.breakpoint)
            {
                reached = tier.breakpoint;
            }
            else
            {
                next = tier.breakpoint;
                break;
            }
        }

        const float progress = next > 0 ? static_cast<float>(std::min(c, next)) / static_cast<float>(next) : 0.0f;
        synergy += progress * AIConstants::AIPlayerTraitProgressWeight;
        if (reached > 0)
        {
            synergy += AIConstants::AIPlayerTraitReachedBonus;
        }
    }
    s.traitSynergy = synergy;

    return s;
}

std::string AIPlayer::choosePrimaryTrait(const PlayerState& player,
                                        const ContentManager& content) const
{
    const auto counts = traitCounts(player, content);
    if (counts.empty())
    {
        return "";
    }

    std::string best;
    int bestCount = -1;
    for (const std::string& t : sortedKeys(counts))
    {
        const int c = counts.at(t);
        if (c > bestCount)
        {
            best = t;
            bestCount = c;
        }
    }
    return best;
}

int AIPlayer::chooseCarryIndex(const PlayerState& player,
                               const ContentManager& content) const
{
    int bestIndex = -1;
    float best = AIConstants::AIPlayerSentinelScore;
    for (std::size_t i = 0; i < player.board().size(); ++i)
    {
        const OwnedUnit& u = player.board()[i];
        const ChampionDefinition* def = content.getChampion(u.championName);
        if (!def)
        {
            continue;
        }
        float score = unitCarryScore(*def);
        for (const std::string& itemName : u.items)
        {
            if (const Item* item = content.getItem(itemName))
            {
                score += itemCarryWeight(*item);
            }
        }
        if (score > best)
        {
            best = score;
            bestIndex = static_cast<int>(i);
        }
    }
    return bestIndex;
}

int AIPlayer::chooseFrontlineIndex(const PlayerState& player,
                                   const ContentManager& content,
                                   int skipIndex) const
{
    int bestIndex = -1;
    float best = AIConstants::AIPlayerSentinelScore;
    for (std::size_t i = 0; i < player.board().size(); ++i)
    {
        if (static_cast<int>(i) == skipIndex)
        {
            continue;
        }
        const OwnedUnit& u = player.board()[i];
        const ChampionDefinition* def = content.getChampion(u.championName);
        if (!def)
        {
            continue;
        }
        float score = unitTankScore(*def);
        for (const std::string& itemName : u.items)
        {
            if (const Item* item = content.getItem(itemName))
            {
                score += itemTankWeight(*item);
            }
        }
        if (score > best)
        {
            best = score;
            bestIndex = static_cast<int>(i);
        }
    }
    return bestIndex;
}

void AIPlayer::applyPositioning(PlayerState& self,
                               const ContentManager& content,
                               const std::vector<PlayerState>& enemies,
                               std::ostream& out) const
{
    if (self.board().empty())
    {
        return;
    }

    float bestEnemyTotal = AIConstants::AIPlayerSentinelScore;
    BoardScore bestEnemyScore{};
    for (const PlayerState& e : enemies)
    {
        BoardScore s = evaluateBoard(e, content);
        const float t = s.total();
        if (t > bestEnemyTotal)
        {
            bestEnemyTotal = t;
            bestEnemyScore = s;
        }
    }

    const int carry = chooseCarryIndex(self, content);
    const int tank1 = chooseFrontlineIndex(self, content, carry);
    const int tank2 = chooseFrontlineIndex(self, content, tank1);

    std::vector<OwnedUnit>& board = self.boardMutable();
    for (OwnedUnit& u : board)
    {
        u.hasFormation = false;
        u.formation = Position{ 0, 0 };
    }

    if (carry >= 0 && carry < static_cast<int>(board.size()))
    {
        OwnedUnit& c = board[static_cast<std::size_t>(carry)];
        c.hasFormation = true;
        c.formation = Position{ 0, 0 };
    }

    if (tank1 >= 0 && tank1 < static_cast<int>(board.size()))
    {
        OwnedUnit& t = board[static_cast<std::size_t>(tank1)];
        t.hasFormation = true;
        t.formation = Position{ GameConstants::DefaultFormationX, GameConstants::FormationHeight - 1 };
    }
    if (tank2 >= 0 && tank2 < static_cast<int>(board.size()) && tank2 != tank1)
    {
        OwnedUnit& t = board[static_cast<std::size_t>(tank2)];
        t.hasFormation = true;
        t.formation = Position{ GameConstants::DefaultFormationX + 1, GameConstants::FormationHeight - 1 };
    }

    for (std::size_t i = 0; i < board.size(); ++i)
    {
        OwnedUnit& u = board[i];
        if (u.hasFormation)
        {
            continue;
        }

        static constexpr int SlotCount = 5;
        static constexpr int SlotBaseX = 1;

        const int slot = static_cast<int>(i) % SlotCount;
        u.hasFormation = true;
        u.formation = Position{ SlotBaseX + slot, GameConstants::DefaultFormationY };
    }

    std::ostringstream ss;
    ss << "AI positions: carry->corner, tanks->front";
    if (!enemies.empty())
    {
        ss << " | scouted best enemy score=" << std::lround(bestEnemyScore.total());
    }
    out << "AI[" << name_ << "] " << ss.str() << "\n";
}

void AIPlayer::applyItemPlacement(PlayerState& self,
                                 const ContentManager& content,
                                 std::ostream& out) const
{
    if (self.board().empty() || self.itemBench().empty())
    {
        return;
    }

    const int carryIndex = chooseCarryIndex(self, content);
    const int tankIndex = chooseFrontlineIndex(self, content, carryIndex);

    int equips = 0;
    while (!self.itemBench().empty() && equips < static_cast<int>(GameConstants::MaxItemsPerUnit))
    {
        std::string itemName = self.itemBench().front();
        const Item* item = content.getItem(itemName);
        if (!item)
        {
            self.itemBenchMutable().erase(self.itemBenchMutable().begin());
            continue;
        }

        const float carryW = itemCarryWeight(*item);
        const float tankW = itemTankWeight(*item);

        std::size_t target = 0;
        if (carryW >= tankW && carryIndex >= 0)
        {
            target = static_cast<std::size_t>(carryIndex);
        }
        else if (tankIndex >= 0)
        {
            target = static_cast<std::size_t>(tankIndex);
        }

        if (!self.equipItemToBoardUnit(target, 0))
        {
            break;
        }

        out << "AI[" << name_ << "] places item " << itemName << " on " << self.board()[target].championName << "\n";
        equips += 1;
    }
}

void AIPlayer::shopBuyPhase(PlayerState& self,
                            const ContentManager& content,
                            ShopSystem& shop,
                            Random&,
                            std::ostream& out) const
{
    std::string primaryTrait = choosePrimaryTrait(self, content);
    if (!targetTrait_.empty())
    {
        primaryTrait = targetTrait_;
    }

    const auto currentTraits = traitCounts(self, content);

    float bestScore = AIConstants::AIPlayerSentinelScore;
    std::size_t bestIndex = static_cast<std::size_t>(-1);
    std::string bestName;

    for (std::size_t i = 0; i < self.shop().size(); ++i)
    {
        const ShopOffer& offer = self.shop()[i];
        if (offer.championName.empty())
        {
            continue;
        }
        if (offer.cost > self.gold())
        {
            continue;
        }
        const ChampionDefinition* def = content.getChampion(offer.championName);
        if (!def)
        {
            continue;
        }

        float score = static_cast<float>(def->cost) * AIConstants::AIPlayerShopCostWeight +
                      unitCarryScore(*def) * AIConstants::AIPlayerShopCarryWeight +
                      unitTankScore(*def) * AIConstants::AIPlayerShopTankWeight;

        for (const std::string& t : def->traits)
        {
            const int c = currentTraits.count(t) ? currentTraits.at(t) : 0;
            const float traitBonus =
                AIConstants::AIPlayerShopTraitBonusBase + static_cast<float>(c) * AIConstants::AIPlayerShopTraitBonusPerCount;
            score += traitBonus;
            if (!primaryTrait.empty() && t == primaryTrait)
            {
                score += AIConstants::AIPlayerShopPrimaryTraitBonus;
            }
        }

        if (score > bestScore)
        {
            bestScore = score;
            bestIndex = i;
            bestName = offer.championName;
        }
    }

    if (bestIndex != static_cast<std::size_t>(-1))
    {
        if (shop.buy(self, bestIndex))
        {
            out << "AI[" << name_ << "] buys " << bestName << "\n";
        }
    }
}

bool AIPlayer::shouldReroll(const PlayerState& self,
                            const PlayerState* enemy,
                            const ContentManager& content) const
{
    if (self.health() <= AIConstants::AIPlayerRerollHealthThreshold && self.gold() >= ShopSystem::rerollCost())
    {
        return true;
    }

    if (self.gold() >= MacroConstants::MaxInterestGold)
    {
        return false;
    }

    if (enemy)
    {
        const float s = evaluateBoard(self, content).total();
        const float e = evaluateBoard(*enemy, content).total();
        if (s + AIConstants::AIPlayerRerollBehindScoreGap < e && self.gold() >= AIConstants::AIPlayerRerollMinGoldWhenBehind)
        {
            return true;
        }
    }

    return false;
}

bool AIPlayer::shouldLevel(const PlayerState& self,
                           const PlayerState* enemy,
                           const ContentManager& content,
                           std::int32_t roundIndex) const
{
    if (self.level() >= GameConstants::MaxLevel)
    {
        return false;
    }

    const int needXp = ShopSystem::xpToNextLevel(self.level());
    if (needXp <= 0)
    {
        return false;
    }

    const int goldToLevelNow = std::max(0, needXp - self.xp());
    if (goldToLevelNow <= 0)
    {
        return true;
    }

    if (self.gold() >= MacroConstants::MaxInterestGold)
    {
        return self.gold() - MacroConstants::MaxInterestGold >= goldToLevelNow;
    }

    if (enemy)
    {
        const float s = evaluateBoard(self, content).total();
        const float e = evaluateBoard(*enemy, content).total();
        if (s + AIConstants::AIPlayerLevelUnderPressureScoreGap < e &&
            self.gold() >= goldToLevelNow + AIConstants::AIPlayerLevelMinExtraGoldUnderPressure)
        {
            return true;
        }
    }

    if (roundIndex >= AIConstants::AIPlayerLevelRoundThreshold &&
        self.gold() >= goldToLevelNow + AIConstants::AIPlayerLevelMinExtraGoldAfterThreshold)
    {
        return true;
    }

    return false;
}

CombatPrediction AIPlayer::predictCombat(const PlayerState& self,
                                        const PlayerState& enemy,
                                        ShopSystem&,
                                        RoundSystem& rounds,
                                        const ContentManager&,
                                        std::int32_t roundIndex,
                                        int simulations,
                                        std::uint32_t seedBase) const
{
    simulations = std::clamp(simulations,
                             AIConstants::AIPlayerPredictCombatMinSimulations,
                             AIConstants::AIPlayerPredictCombatMaxSimulations);
    int wins = 0;
    float survivorDelta = 0.0f;

    for (int i = 0; i < simulations; ++i)
    {
        PlayerState a = self;
        PlayerState b = enemy;

        const std::uint32_t simSeed = mixSeed(seedBase, static_cast<std::uint32_t>(i));
        RoundResult r = rounds.runPvP(a, b, roundIndex, simSeed);
        if (r.playerAWon)
        {
            wins += 1;
        }
        survivorDelta += static_cast<float>(r.survivingA - r.survivingB);
    }

    CombatPrediction p{};
    p.winChance = static_cast<float>(wins) / static_cast<float>(simulations);
    p.avgSurvivorDelta = survivorDelta / static_cast<float>(simulations);
    return p;
}

void AIPlayer::takeTurn(PlayerState& self,
                        const std::vector<PlayerState>& enemies,
                        ShopSystem& shop,
                        RoundSystem& rounds,
                        const ContentManager& content,
                        std::int32_t roundIndex,
                        Random& rng,
                        std::ostream& out)
{
    const PlayerState* enemy = enemies.empty() ? nullptr : &enemies.front();
    if (!enemy && !enemies.empty())
    {
        enemy = &enemies.front();
    }
    if (enemy && enemies.size() > 1)
    {
        float best = -1.0f;
        for (const PlayerState& e : enemies)
        {
            float score = evaluateBoard(e, content).total();
            if (score > best)
            {
                best = score;
                enemy = &e;
            }
        }
    }

    const std::uint32_t turnSeed = mixSeed(seed_, static_cast<std::uint32_t>(roundIndex));

    CombatPrediction pred{};
    bool hasPred = false;
    if (enemy)
    {
        pred = predictCombat(self, *enemy, shop, rounds, content, roundIndex, AIConstants::AIPlayerPredictCombatDefaultSimulations, turnSeed);
        hasPred = true;
        out << "AI[" << name_ << "] predicts " << std::lround(pred.winChance * AIConstants::PercentScale) << "% combat win chance\n";
    }

    const bool tempo = hasPred && pred.winChance < AIConstants::AIPlayerTempoWinChanceThreshold;
    const bool rollDown = hasPred && pred.winChance < AIConstants::AIPlayerRollDownWinChanceThreshold;

    const std::string primary = choosePrimaryTrait(self, content);
    const std::string before = targetTrait_;

    if (targetTrait_.empty())
    {
        targetTrait_ = primary;
    }
    else if (!primary.empty() && primary != targetTrait_)
    {
        const auto counts = traitCounts(self, content);
        const int targetCount = counts.count(targetTrait_) ? counts.at(targetTrait_) : 0;
        const int primaryCount = counts.count(primary) ? counts.at(primary) : 0;
        if (primaryCount > targetCount)
        {
            targetTrait_ = primary;
        }
    }

    if (!targetTrait_.empty() && targetTrait_ != before)
    {
        out << "AI[" << name_ << "] pivots into " << targetTrait_ << " comp\n";
    }

    const bool wantsLevel =
        shouldLevel(self, enemy, content, roundIndex) ||
        (tempo && self.level() < GameConstants::MaxLevel &&
         static_cast<int>(self.board().size()) >= self.unitCap() &&
         self.gold() >= AIConstants::AIPlayerTempoLevelMinGold);

    if (wantsLevel)
    {
        const int need = ShopSystem::xpToNextLevel(self.level());
        const int goldToLevelNow = std::max(0, need - self.xp());
        const int reserve = (self.health() <= AIConstants::AIPlayerRerollHealthThreshold || tempo) ? 0 : MacroConstants::MaxInterestGold;
        const int spend = std::max(0, std::min(goldToLevelNow, static_cast<int>(self.gold()) - reserve));
        if (spend > 0)
        {
            out << "AI[" << name_ << "] chooses to level to " << (self.level() + 1) << "\n";
            rounds.buyXp(self, spend);
            rounds.autoLevel(self);
        }
        else if (self.xp() >= need)
        {
            out << "AI[" << name_ << "] chooses to level to " << (self.level() + 1) << "\n";
            rounds.autoLevel(self);
        }
    }

    shopBuyPhase(self, content, shop, rng, out);

    if (shouldReroll(self, enemy, content) || rollDown)
    {
        const int maxRolls = rollDown ? AIConstants::AIPlayerRollDownMaxRerolls
                                      : (self.health() <= AIConstants::AIPlayerRerollHealthThreshold ? AIConstants::AIPlayerStabilizeMaxRerolls : 1);
        for (int i = 0; i < maxRolls; ++i)
        {
            if (self.gold() < ShopSystem::rerollCost())
            {
                break;
            }
            shop.reroll(self, rng, true);
            out << "AI[" << name_ << "] rerolls shop\n";
            shopBuyPhase(self, content, shop, rng, out);
        }
    }

    while (static_cast<int>(self.board().size()) < self.unitCap() && !self.bench().empty())
    {
        self.moveBenchToBoard(0);
    }
    while (static_cast<int>(self.board().size()) > self.unitCap())
    {
        self.moveBoardToBench(self.board().size() - 1);
    }

    applyItemPlacement(self, content, out);
    applyPositioning(self, content, enemies, out);
}
