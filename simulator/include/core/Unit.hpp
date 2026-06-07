    // Unit.hpp
    #pragma once

    #include <vector>
    #include <string>
    #include <string_view>
    #include <cstdint>
#include "content/Ability.hpp"
#include "content/Item.hpp"
#include "core/Position.hpp"
#include "core/Board.hpp"
#include "core/TeamId.hpp"
#include "combat/DamageType.hpp"
#include "combat/StatModifier.hpp"
#include "combat/StatusEffect.hpp"
    class Unit {
    public:
        
        // Constructor: unit with no ability (mana disabled)
        
        Unit(std::string name,
            std::int32_t hp,
            std::int32_t ad,
            std::int32_t armor,
            std::int32_t magicResist,
            std::int32_t attackCooldownMs,
            std::int32_t attackRange,
            DamageType autoAttackDamageType,
            Position position,
            TeamId teamId);
        
        // Constructor: unit with ability and mana

        
        Unit(std::string name,
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
            Ability ability);

        
        // Combat actions
        
        std::int32_t attack(Unit& target);   // Normal attack — gains mana, resets timer
        void applyDamage(std::int32_t damage);
        void heal(std::int32_t amount);

        
        // Queries
        
        bool isAlive() const;
        bool canAttack() const;
        bool isInRange(const Unit& target) const;
        TeamId getTeamId() const;
        bool isEnemyOf(const Unit& other) const;

        
        // Mana
        
        std::int32_t getMana() const;
        std::int32_t getMaxMana() const;
        std::int32_t getManaGainOnAttack() const;
        bool canGainMana() const;
        void gainMana(std::int32_t amount);
        void resetMana();
        void resetManaAfterCast();
        bool canCastAbility() const;
        bool isCasting() const;
        bool isManaLocked() const;
        void beginCast(std::int32_t windupMs, std::int32_t recoveryMs);
        void endCast();
        void interruptCast();

        
        // Ability
        
        const Ability& getAbility() const;

        
        // Timing / movement
        
        void tick(std::int32_t dtMs);
        void resetAttackTimer();
        bool moveToward(const Unit& target, Board& board);
        void setMovedThisTurn(bool value);
        bool didMoveThisTurn() const;
        void setCastThisTurn(bool value);
        bool didCastThisTurn() const;
        void setAttackedThisTurn(bool value);
        bool didAttackThisTurn() const;

        void addStatModifier(const StatModifier& modifier);
        void clearStatModifiers();
        const std::vector<StatModifier>& statModifiers() const;

        void addStatusEffect(const StatusEffect& effect);
        void updateStatusEffects(std::int32_t deltaMs);
        const std::vector<StatusEffect>& statusEffects() const;
        std::vector<StatusEffect>& statusEffectsMutable();
        bool hasCrowdControl(CrowdControlType crowdControlType) const;
        bool canMoveNow() const;
        bool canAutoAttackNow() const;
        bool canCastNow() const;
        bool isUntargetable() const;

        void beginAttackLock(std::int32_t lockMs);


        DamageType getAutoAttackDamageType() const;
        
        // Getters
        
        const std::string& getName() const;
        std::int32_t getHp() const;
        std::int32_t getMaxHp() const;
        std::int32_t getAd() const;
        float getAbilityPower() const;
        std::int32_t getArmor() const;
        std::int32_t getMagicResist() const;
        float getAttackSpeed() const;
        float getCritChance() const;
        float getCritDamage() const;
        float getDurability() const;
        std::int32_t getAttackCooldownMs() const;
        std::int32_t getAttackRange() const;
        std::int32_t getAttackTimerMs() const;
        Position getPosition() const;
        Position getLastPosition() const;
        void applyMove(const Position& newPosition);
        std::int32_t getLastBlockedWarnAtMs() const;
        void setLastBlockedWarnAtMs(std::int32_t timeMs);

        void setHp(std::int32_t hp);
        void setMaxHp(std::int32_t maxHp);
        void setAd(std::int32_t ad);
        void setAbilityPower(float abilityPower);
        void setArmor(std::int32_t armor);
        void setMagicResist(std::int32_t magicResist);
        void setAttackSpeed(float attackSpeed);
        void setCritChance(float critChance);
        void setCritDamage(float critDamage);
        void setDurability(float durability);

        const std::vector<std::string>& traits() const;
        void setTraits(std::vector<std::string> traits);
        void addTrait(std::string traitName);
        bool hasTrait(std::string_view traitName) const;

        const std::vector<Item>& items() const;
        bool addItem(const Item& item);

    private:
        std::string  name_;
        TeamId       team_id_;
        std::int32_t ad_;
        float ability_power_;
        std::int32_t armor_;
        std::int32_t magic_resist_;
        float attack_speed_;
        float crit_chance_;
        float crit_damage_;
        float durability_;

        // HP
        std::int32_t hp_;
        std::int32_t max_hp_;

        // Mana
        std::int32_t mana_; 
        std::int32_t max_mana_; 
        std::int32_t mana_gain_on_attack_; // mana earned each time this unit attacks

        Ability ability_;

        // Attack timing
        std::int32_t attack_cooldown_ms_;
        std::int32_t attack_timer_ms_;

        // Action locks / casting
        bool casting_;
        bool mana_locked_;
        bool cast_mana_reset_done_;
        std::int32_t cast_windup_remaining_ms_;
        std::int32_t cast_recovery_remaining_ms_;
        std::int32_t attack_lock_remaining_ms_;

        // Attack range
        std::int32_t attack_range_;
        Position     position_;
        Position     last_position_;
        bool         moved_this_turn_;
        bool         cast_this_turn_;
        bool         attacked_this_turn_;
        std::int32_t last_blocked_warn_at_ms_;

        DamageType auto_attack_damage_type_;
        std::vector<StatModifier> stat_modifiers_;
        std::vector<StatusEffect> status_effects_;
        std::vector<std::string> traits_;
        std::vector<Item> items_;

    };
