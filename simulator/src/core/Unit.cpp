// Unit.cpp
#include "core/Unit.hpp"
#include "combat/MovementSystem.hpp"
#include "combat/DamageSystem.hpp"
#include "combat/DamageType.hpp"
#include "combat/StatSystem.hpp"
#include "constants/CombatConstants.hpp"
#include <algorithm>
#include <cmath>
#include <utility>


Unit::Unit(std::string name,
           std::int32_t hp,
           std::int32_t ad,
           std::int32_t armor,
           std::int32_t magicResist,
           std::int32_t attackCooldownMs,
           std::int32_t attackRange,
           DamageType autoAttackDamageType,
           Position position,
           TeamId teamId)
    : name_(std::move(name)),
      team_id_(teamId),
      ad_(ad),
      ability_power_(0.0f),
      armor_(armor),
      magic_resist_(magicResist),
      attack_speed_(attackCooldownMs > 0
                        ? (static_cast<float>(CombatConstants::MsPerSecond) / static_cast<float>(attackCooldownMs))
                        : 1.0f),
      crit_chance_(0.0f),
      crit_damage_(CombatConstants::DefaultCritDamageMultiplier),
      durability_(0.0f),
      auto_attack_damage_type_(autoAttackDamageType),  
      hp_(hp),
      max_hp_(hp),
      mana_(0),
      max_mana_(0),
      mana_gain_on_attack_(0),
      ability_(Ability{}),
      attack_cooldown_ms_(attackCooldownMs),
      attack_timer_ms_(0),
      casting_(false),
      mana_locked_(false),
      cast_mana_reset_done_(false),
      cast_windup_remaining_ms_(0),
      cast_recovery_remaining_ms_(0),
      attack_lock_remaining_ms_(0),
      attack_range_(attackRange),
      position_(position),
      last_position_(position),
      moved_this_turn_(false),
      cast_this_turn_(false),
      attacked_this_turn_(false),
      last_blocked_warn_at_ms_(CombatConstants::BlockedWarnInitialTimeMs),
      traits_{},
      items_{}{
}


Unit::Unit(std::string name,
           std::int32_t hp,
           std::int32_t ad,
           std::int32_t armor,
           std::int32_t magicResist,
           std::int32_t attackCooldownMs,
           std::int32_t attackRange,
           DamageType autoAttackDamageType,
           Position position,
           TeamId teamId,
           std::int32_t maxMana,
           std::int32_t manaGainOnAttack,
           Ability ability)
    : name_(std::move(name)),
      team_id_(teamId),
      ad_(ad),
      ability_power_(0.0f),
      armor_(armor),
      magic_resist_(magicResist),
      attack_speed_(attackCooldownMs > 0
                        ? (static_cast<float>(CombatConstants::MsPerSecond) / static_cast<float>(attackCooldownMs))
                        : 1.0f),
      crit_chance_(0.0f),
      crit_damage_(CombatConstants::DefaultCritDamageMultiplier),
      durability_(0.0f),
      auto_attack_damage_type_(autoAttackDamageType),  
      hp_(hp),
      max_hp_(hp),
      mana_(0),
      max_mana_(maxMana < 0 ? 0 : maxMana),
      mana_gain_on_attack_(manaGainOnAttack < 0 ? 0 : manaGainOnAttack),
      ability_(std::move(ability)),
      attack_cooldown_ms_(attackCooldownMs),
      attack_timer_ms_(0),
      casting_(false),
      mana_locked_(false),
      cast_mana_reset_done_(false),
      cast_windup_remaining_ms_(0),
      cast_recovery_remaining_ms_(0),
      attack_lock_remaining_ms_(0),
      attack_range_(attackRange),
      position_(position),
      last_position_(position),
      moved_this_turn_(false),
      cast_this_turn_(false),
      attacked_this_turn_(false),

      last_blocked_warn_at_ms_(CombatConstants::BlockedWarnInitialTimeMs),
      traits_{},
      items_{}
{
}


bool Unit::isInRange(const Unit& target) const
{
    return distanceBetween(position_, target.getPosition()) <= attack_range_;
}


bool Unit::moveToward(const Unit& target, Board& board)
{
    return MovementSystem::tryMoveToward(*this, target, board);
}



DamageType Unit::getAutoAttackDamageType() const
{
    return auto_attack_damage_type_;
}

//DamageSystem calculates final damage.
std::int32_t Unit::attack(Unit& target)
{
    const std::int32_t rawDamage =
        StatSystem::getFinalStatInt(*this, StatType::AttackDamage);

    const std::int32_t damage =
        DamageSystem::calculateDamage(rawDamage, auto_attack_damage_type_, target);

    target.applyDamage(damage);

    gainMana(StatSystem::getFinalStatInt(*this, StatType::ManaGainOnAttack));
    resetAttackTimer();

    return damage;
}






// Unit::applyDamage subtracts HP.
void Unit::applyDamage(std::int32_t damage)
{
    if (damage <= 0)
    {
        return;
    }

    for (StatusEffect& effect : status_effects_)
    {
        if (damage <= 0)
        {
            break;
        }

        if (effect.effectType != StatusEffectType::Shield)
        {
            continue;
        }

        if (effect.remainingMs <= 0)
        {
            continue;
        }

        if (effect.value <= 0.0f)
        {
            continue;
        }

        const std::int32_t shield = static_cast<std::int32_t>(effect.value);
        if (shield <= 0)
        {
            continue;
        }

        const std::int32_t absorbed = std::min<std::int32_t>(damage, shield);
        damage -= absorbed;
        effect.value = static_cast<float>(shield - absorbed);
    }

    if (damage <= 0)
    {
        return;
    }

    hp_ = std::max<std::int32_t>(0, hp_ - damage);
}


void Unit::heal(std::int32_t amount)
{
    if (amount <= 0)
    {
        return;
    }
    const std::int32_t effectiveMaxHp =
        std::max<std::int32_t>(1, StatSystem::getFinalStatInt(*this, StatType::MaxHp));
    hp_ = std::min<std::int32_t>(effectiveMaxHp, hp_ + amount);
}


bool Unit::isAlive() const
{
    return hp_ > 0;
}

std::int32_t Unit::getArmor() const
{
    return armor_;
}

float Unit::getAbilityPower() const
{
    return ability_power_;
}

std::int32_t Unit::getMagicResist() const
{
    return magic_resist_;
}

float Unit::getAttackSpeed() const
{
    return attack_speed_;
}

float Unit::getCritChance() const
{
    return crit_chance_;
}

float Unit::getCritDamage() const
{
    return crit_damage_;
}

float Unit::getDurability() const
{
    return durability_;
}


TeamId Unit::getTeamId() const
{
    return team_id_;
}

bool Unit::isEnemyOf(const Unit& other) const
{
    return team_id_ != other.team_id_;
}


std::int32_t Unit::getMana() const
{
    return mana_;
}

std::int32_t Unit::getMaxMana() const
{
    return max_mana_;
}

std::int32_t Unit::getManaGainOnAttack() const
{
    return mana_gain_on_attack_;
}

bool Unit::canGainMana() const
{
    return !casting_ && !mana_locked_ && max_mana_ > 0;
}

void Unit::gainMana(std::int32_t amount)
{
    if (!canGainMana())
    {
        return;
    }
    if (amount <= 0)
    {
        return;
    }
    mana_ = std::min<std::int32_t>(max_mana_, mana_ + amount);
}

void Unit::resetMana()
{
    mana_ = 0;
}

void Unit::resetManaAfterCast()
{
    if (cast_mana_reset_done_)
    {
        return;
    }
    mana_ = 0;
    cast_mana_reset_done_ = true;
}


bool Unit::canCastAbility() const
{
    return max_mana_ > 0
        && ability_.manaCost > 0
        && mana_ >= ability_.manaCost
        && !ability_.effects.empty();
}


const Ability& Unit::getAbility() const
{
    return ability_;
}


bool Unit::canAttack() const
{
    const float attackSpeed = StatSystem::getFinalStat(*this, StatType::AttackSpeed);
    if (attackSpeed <= 0.0f)
    {
        return false;
    }

    const std::int32_t effectiveCooldownMs =
        std::max<std::int32_t>(
            1,
            static_cast<std::int32_t>(std::lround(static_cast<float>(CombatConstants::MsPerSecond) / attackSpeed)));

    if (effectiveCooldownMs <= 0)
    {
        return true;
    }
    return attack_timer_ms_ >= effectiveCooldownMs;
}

void Unit::tick(std::int32_t dtMs)
{
    if (dtMs > 0)
    {
        if (attack_lock_remaining_ms_ > 0)
        {
            attack_lock_remaining_ms_ = std::max<std::int32_t>(0, attack_lock_remaining_ms_ - dtMs);
        }

        if (casting_)
        {
            if (cast_windup_remaining_ms_ > 0)
            {
                cast_windup_remaining_ms_ = std::max<std::int32_t>(0, cast_windup_remaining_ms_ - dtMs);
            }
            else if (cast_recovery_remaining_ms_ > 0)
            {
                cast_recovery_remaining_ms_ = std::max<std::int32_t>(0, cast_recovery_remaining_ms_ - dtMs);
            }

            if (cast_windup_remaining_ms_ == 0 && cast_recovery_remaining_ms_ == 0)
            {
                casting_ = false;
                mana_locked_ = false;
            }
        }
    }

    const float attackSpeed = StatSystem::getFinalStat(*this, StatType::AttackSpeed);
    if (attackSpeed <= 0.0f)
    {
        return;
    }
    if (dtMs <= 0)
    {
        return;
    }

    const std::int32_t effectiveCooldownMs =
        std::max<std::int32_t>(
            1,
            static_cast<std::int32_t>(std::lround(static_cast<float>(CombatConstants::MsPerSecond) / attackSpeed)));

    attack_timer_ms_ += dtMs;

    if (attack_timer_ms_ > effectiveCooldownMs)
    {
        attack_timer_ms_ = effectiveCooldownMs;
    }
}

void Unit::resetAttackTimer()
{
    attack_timer_ms_ = 0;
}


void Unit::applyMove(const Position& newPosition)
{
    last_position_ = position_;
    position_ = newPosition;
    moved_this_turn_ = true;
}

void Unit::setMovedThisTurn(bool value)
{
    moved_this_turn_ = value;
}

bool Unit::didMoveThisTurn() const
{
    return moved_this_turn_;
}

void Unit::setCastThisTurn(bool value)
{
    cast_this_turn_ = value;
}

bool Unit::didCastThisTurn() const
{
    return cast_this_turn_;
}

void Unit::setAttackedThisTurn(bool value)
{
    attacked_this_turn_ = value;
}

bool Unit::didAttackThisTurn() const
{
    return attacked_this_turn_;
}

void Unit::addStatModifier(const StatModifier& modifier)
{
    stat_modifiers_.push_back(modifier);
}

void Unit::clearStatModifiers()
{
    stat_modifiers_.clear();
}

const std::vector<StatModifier>& Unit::statModifiers() const
{
    return stat_modifiers_;
}

void Unit::addStatusEffect(const StatusEffect& effect)
{
    StatusEffect e = effect;
    if (e.durationMs < 0)
    {
        e.durationMs = 0;
    }
    if (e.remainingMs <= 0)
    {
        e.remainingMs = e.durationMs;
    }
    if (e.remainingMs > 0)
    {
        e.remainingMs += 1;
    }
    if (e.tickIntervalMs < 0)
    {
        e.tickIntervalMs = 0;
    }
    if (e.tickTimerMs < 0)
    {
        e.tickTimerMs = 0;
    }
    status_effects_.push_back(std::move(e));
}

void Unit::updateStatusEffects(std::int32_t deltaMs)
{
    if (deltaMs <= 0)
    {
        return;
    }

    for (StatusEffect& effect : status_effects_)
    {
        if (effect.remainingMs <= 0)
        {
            continue;
        }

        const std::int32_t timeToProcess = std::min<std::int32_t>(deltaMs, effect.remainingMs);
        effect.remainingMs -= timeToProcess;

        if (effect.tickIntervalMs > 0)
        {
            effect.tickTimerMs += timeToProcess;
        }

        const bool isDot = effect.effectType == StatusEffectType::DamageOverTime;
        const bool isHot = effect.effectType == StatusEffectType::HealOverTime;

        if ((isDot || isHot) && effect.tickIntervalMs > 0)
        {
            while (effect.tickTimerMs >= effect.tickIntervalMs)
            {
                effect.tickTimerMs -= effect.tickIntervalMs;

                const std::int32_t amount = static_cast<std::int32_t>(effect.value);
                if (amount <= 0)
                {
                    continue;
                }

                if (isDot)
                {
                    applyDamage(DamageSystem::calculateDamage(amount, effect.damageType, *this));
                }
                else
                {
                    heal(amount);
                }
            }
        }
    }

    status_effects_.erase(
        std::remove_if(
            status_effects_.begin(),
            status_effects_.end(),
            [](const StatusEffect& e)
            {
                return e.remainingMs <= 0;
            }
        ),
        status_effects_.end()
    );

    if (casting_ && (hasCrowdControl(CrowdControlType::Stun) ||
                     hasCrowdControl(CrowdControlType::Knockup) ||
                     hasCrowdControl(CrowdControlType::Suppression)))
    {
        interruptCast();
    }
}

const std::vector<StatusEffect>& Unit::statusEffects() const
{
    return status_effects_;
}

std::vector<StatusEffect>& Unit::statusEffectsMutable()
{
    return status_effects_;
}

bool Unit::hasCrowdControl(CrowdControlType crowdControlType) const
{
    if (crowdControlType == CrowdControlType::None)
    {
        return false;
    }

    for (const StatusEffect& effect : status_effects_)
    {
        if (effect.remainingMs <= 0)
        {
            continue;
        }

        if (effect.effectType != StatusEffectType::CrowdControl)
        {
            continue;
        }

        if (effect.crowdControlType == crowdControlType)
        {
            return true;
        }
    }

    return false;
}

bool Unit::canMoveNow() const
{
    if (casting_ || attack_lock_remaining_ms_ > 0)
    {
        return false;
    }
    if (hasCrowdControl(CrowdControlType::Stun))
    {
        return false;
    }
    if (hasCrowdControl(CrowdControlType::Knockup))
    {
        return false;
    }
    if (hasCrowdControl(CrowdControlType::Suppression))
    {
        return false;
    }
    if (hasCrowdControl(CrowdControlType::Root))
    {
        return false;
    }
    return true;
}

bool Unit::canAutoAttackNow() const
{
    if (casting_ || attack_lock_remaining_ms_ > 0)
    {
        return false;
    }
    if (hasCrowdControl(CrowdControlType::Stun))
    {
        return false;
    }
    if (hasCrowdControl(CrowdControlType::Knockup))
    {
        return false;
    }
    if (hasCrowdControl(CrowdControlType::Suppression))
    {
        return false;
    }
    if (hasCrowdControl(CrowdControlType::Fear))
    {
        return false;
    }
    if (hasCrowdControl(CrowdControlType::Disarm))
    {
        return false;
    }
    return true;
}

bool Unit::canCastNow() const
{
    if (casting_ || attack_lock_remaining_ms_ > 0)
    {
        return false;
    }
    if (hasCrowdControl(CrowdControlType::Stun))
    {
        return false;
    }
    if (hasCrowdControl(CrowdControlType::Knockup))
    {
        return false;
    }
    if (hasCrowdControl(CrowdControlType::Suppression))
    {
        return false;
    }
    if (hasCrowdControl(CrowdControlType::Fear))
    {
        return false;
    }
    if (hasCrowdControl(CrowdControlType::Silence))
    {
        return false;
    }
    return true;
}

bool Unit::isCasting() const
{
    return casting_;
}

bool Unit::isManaLocked() const
{
    return mana_locked_;
}

void Unit::beginCast(std::int32_t windupMs, std::int32_t recoveryMs)
{
    casting_ = true;
    mana_locked_ = true;
    cast_mana_reset_done_ = false;
    cast_windup_remaining_ms_ = std::max<std::int32_t>(0, windupMs);
    cast_recovery_remaining_ms_ = std::max<std::int32_t>(0, recoveryMs);
}

void Unit::endCast()
{
    casting_ = false;
    mana_locked_ = false;
    cast_windup_remaining_ms_ = 0;
    cast_recovery_remaining_ms_ = 0;
}

void Unit::interruptCast()
{
    endCast();
}

bool Unit::isUntargetable() const
{
    for (const StatusEffect& effect : status_effects_)
    {
        if (effect.remainingMs <= 0)
        {
            continue;
        }
        if (effect.name == "Untargetable")
        {
            return true;
        }
    }
    return false;
}

void Unit::beginAttackLock(std::int32_t lockMs)
{
    attack_lock_remaining_ms_ = std::max<std::int32_t>(
        attack_lock_remaining_ms_,
        std::max<std::int32_t>(0, lockMs)
    );
}


const std::string& Unit::getName() const
{
    return name_;
}

std::int32_t Unit::getHp() const
{
    return hp_;
}

std::int32_t Unit::getMaxHp() const
{
    return max_hp_;
}

std::int32_t Unit::getAd() const
{
    return ad_;
}

void Unit::setHp(std::int32_t hp)
{
    const std::int32_t effectiveMaxHp =
        std::max<std::int32_t>(1, StatSystem::getFinalStatInt(*this, StatType::MaxHp));
    hp_ = std::clamp<std::int32_t>(hp, 0, effectiveMaxHp);
}

void Unit::setMaxHp(std::int32_t maxHp)
{
    max_hp_ = std::max<std::int32_t>(1, maxHp);
    hp_ = std::min<std::int32_t>(hp_, max_hp_);
}

void Unit::setAd(std::int32_t ad)
{
    ad_ = std::max<std::int32_t>(0, ad);
}

void Unit::setAbilityPower(float abilityPower)
{
    ability_power_ = std::max(0.0f, abilityPower);
}

void Unit::setArmor(std::int32_t armor)
{
    armor_ = std::max<std::int32_t>(0, armor);
}

void Unit::setMagicResist(std::int32_t magicResist)
{
    magic_resist_ = std::max<std::int32_t>(0, magicResist);
}

void Unit::setAttackSpeed(float attackSpeed)
{
    attack_speed_ = std::max(0.0f, attackSpeed);
}

void Unit::setCritChance(float critChance)
{
    crit_chance_ = std::clamp(critChance, 0.0f, 1.0f);
}

void Unit::setCritDamage(float critDamage)
{
    crit_damage_ = std::max(1.0f, critDamage);
}

void Unit::setDurability(float durability)
{
    durability_ = std::clamp(durability, 0.0f, CombatConstants::MaxDamageReduction);
}

const std::vector<std::string>& Unit::traits() const
{
    return traits_;
}

void Unit::setTraits(std::vector<std::string> traits)
{
    traits_ = std::move(traits);
}

void Unit::addTrait(std::string traitName)
{
    if (traitName.empty())
    {
        return;
    }
    for (const std::string& existing : traits_)
    {
        if (existing == traitName)
        {
            return;
        }
    }
    traits_.push_back(std::move(traitName));
}

bool Unit::hasTrait(std::string_view traitName) const
{
    for (const std::string& t : traits_)
    {
        if (t == traitName)
        {
            return true;
        }
    }
    return false;
}

const std::vector<Item>& Unit::items() const
{
    return items_;
}

bool Unit::addItem(const Item& item)
{
    if (item.name.empty())
    {
        return false;
    }
    if (items_.size() >= 3)
    {
        return false;
    }
    items_.push_back(item);
    return true;
}


std::int32_t Unit::getAttackRange() const
{
    return attack_range_;
}

Position Unit::getPosition() const
{
    return position_;
}

Position Unit::getLastPosition() const
{
    return last_position_;
}

std::int32_t Unit::getLastBlockedWarnAtMs() const
{
    return last_blocked_warn_at_ms_;
}

void Unit::setLastBlockedWarnAtMs(std::int32_t timeMs)
{
    last_blocked_warn_at_ms_ = timeMs;
}

std::int32_t Unit::getAttackCooldownMs() const
{
    return attack_cooldown_ms_;
}

std::int32_t Unit::getAttackTimerMs() const
{
    return attack_timer_ms_;
}
