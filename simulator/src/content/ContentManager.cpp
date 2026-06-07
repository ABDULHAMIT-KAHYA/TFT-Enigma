#include "content/ContentManager.hpp"
#include "combat/CrowdControlType.hpp"
#include "content/ChampionFilter.hpp"
#include "constants/CombatConstants.hpp"
#include "nlohmann/json.hpp"
#include "combat/ModifierType.hpp"
#include "combat/StatType.hpp"
#include "combat/StatusEffectType.hpp"
#include "content/Trait.hpp"
#include "content/UnitFactory.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

static std::string readFileToString(const std::filesystem::path& path)
{
    std::ifstream f(path, std::ios::in | std::ios::binary);
    if (!f)
    {
        throw std::runtime_error("Failed to open file: " + path.string());
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static std::string requiredString(const JsonValue& obj, std::string_view key)
{
    const JsonValue& v = obj.at(key);
    if (!v.isString())
    {
        throw std::runtime_error("Expected string for key: " + std::string(key));
    }
    return v.asString();
}

static std::int32_t requiredInt(const JsonValue& obj, std::string_view key)
{
    const JsonValue& v = obj.at(key);
    if (!v.isNumber())
    {
        throw std::runtime_error("Expected number for key: " + std::string(key));
    }
    return static_cast<std::int32_t>(std::lround(v.asNumber()));
}

static float requiredFloat(const JsonValue& obj, std::string_view key)
{
    const JsonValue& v = obj.at(key);
    if (!v.isNumber())
    {
        throw std::runtime_error("Expected number for key: " + std::string(key));
    }
    return static_cast<float>(v.asNumber());
}

static bool hasKey(const JsonValue& obj, std::string_view key)
{
    return obj.isObject() && obj.hasKey(key);
}

static std::int32_t optionalInt(const JsonValue& obj, std::string_view key, std::int32_t fallback)
{
    if (!hasKey(obj, key))
    {
        return fallback;
    }
    const JsonValue& v = obj.at(key);
    if (!v.isNumber())
    {
        return fallback;
    }
    return static_cast<std::int32_t>(std::lround(v.asNumber()));
}

static float optionalFloat(const JsonValue& obj, std::string_view key, float fallback)
{
    if (!hasKey(obj, key))
    {
        return fallback;
    }
    const JsonValue& v = obj.at(key);
    if (!v.isNumber())
    {
        return fallback;
    }
    return static_cast<float>(v.asNumber());
}

static std::string optionalString(const JsonValue& obj, std::string_view key, std::string fallback)
{
    if (!hasKey(obj, key))
    {
        return fallback;
    }
    const JsonValue& v = obj.at(key);
    if (!v.isString())
    {
        return fallback;
    }
    return v.asString();
}

static std::string trimEdges(std::string s)
{
    auto isAsciiSpace = [](unsigned char c) -> bool { return std::isspace(c) != 0; };
    auto hasLeadingNbsp = [](const std::string& x) -> bool {
        return x.size() >= 2
            && static_cast<unsigned char>(x[0]) == 0xC2
            && static_cast<unsigned char>(x[1]) == 0xA0;
    };
    auto hasTrailingNbsp = [](const std::string& x) -> bool {
        return x.size() >= 2
            && static_cast<unsigned char>(x[x.size() - 2]) == 0xC2
            && static_cast<unsigned char>(x[x.size() - 1]) == 0xA0;
    };

    while (!s.empty())
    {
        const unsigned char c = static_cast<unsigned char>(s.front());
        if (isAsciiSpace(c))
        {
            s.erase(s.begin());
            continue;
        }
        if (hasLeadingNbsp(s))
        {
            s.erase(0, 2);
            continue;
        }
        break;
    }

    while (!s.empty())
    {
        const unsigned char c = static_cast<unsigned char>(s.back());
        if (isAsciiSpace(c))
        {
            s.pop_back();
            continue;
        }
        if (hasTrailingNbsp(s))
        {
            s.resize(s.size() - 2);
            continue;
        }
        break;
    }

    return s;
}

static bool optionalBool(const JsonValue& obj, std::string_view key, bool fallback)
{
    if (!hasKey(obj, key))
    {
        return fallback;
    }
    const JsonValue& v = obj.at(key);
    if (!v.isBool())
    {
        return fallback;
    }
    return v.asBool();
}

static DamageType parseDamageType(std::string_view s)
{
    if (s == "Physical") return DamageType::Physical;
    if (s == "Magic") return DamageType::Magic;
    if (s == "True" || s == "TrueDamage") return DamageType::TrueDamage;
    throw std::runtime_error("Unknown DamageType: " + std::string(s));
}

static AbilityTrigger parseAbilityTrigger(std::string_view s)
{
    if (s == "Passive") return AbilityTrigger::Passive;
    if (s == "OnCombatStart") return AbilityTrigger::OnCombatStart;
    if (s == "OnAttack") return AbilityTrigger::OnAttack;
    if (s == "OnCast") return AbilityTrigger::OnCast;
    if (s == "OnHit") return AbilityTrigger::OnHit;
    if (s == "OnCrit") return AbilityTrigger::OnCrit;
    if (s == "OnKill") return AbilityTrigger::OnKill;
    if (s == "OnDeath") return AbilityTrigger::OnDeath;
    if (s == "OnLowHealth") return AbilityTrigger::OnLowHealth;
    if (s == "OnDamageTaken") return AbilityTrigger::OnDamageTaken;
    throw std::runtime_error("Unknown AbilityTrigger: " + std::string(s));
}

static TargetType parseTargetType(std::string_view s)
{
    if (s == "CurrentEnemy") return TargetType::CurrentEnemy;
    if (s == "Self") return TargetType::Self;
    if (s == "LowestHpAlly") return TargetType::LowestHpAlly;
    if (s == "NearestEnemy") return TargetType::NearestEnemy;
    throw std::runtime_error("Unknown TargetType: " + std::string(s));
}

static AreaShape parseAreaShape(std::string_view s)
{
    if (s == "SingleTarget") return AreaShape::SingleTarget;
    if (s == "Self") return AreaShape::Self;
    if (s == "CircleRadius") return AreaShape::CircleRadius;
    if (s == "Line") return AreaShape::Line;
    if (s == "Cross") return AreaShape::Cross;
    if (s == "Grid") return AreaShape::Grid;
    if (s == "Cone") return AreaShape::Cone;
    throw std::runtime_error("Unknown AreaShape: " + std::string(s));
}

static StatusEffectType parseStatusEffectType(std::string_view s)
{
    if (s == "Buff") return StatusEffectType::Buff;
    if (s == "Debuff") return StatusEffectType::Debuff;
    if (s == "DamageReduction") return StatusEffectType::DamageReduction;
    if (s == "BonusAttackDamage") return StatusEffectType::BonusAttackDamage;
    if (s == "BonusAbilityPower") return StatusEffectType::BonusAbilityPower;
    if (s == "BonusAttackSpeed") return StatusEffectType::BonusAttackSpeed;
    if (s == "BonusArmor") return StatusEffectType::BonusArmor;
    if (s == "BonusMagicResist") return StatusEffectType::BonusMagicResist;
    if (s == "CritChanceBonus") return StatusEffectType::CritChanceBonus;
    if (s == "CritDamageBonus") return StatusEffectType::CritDamageBonus;
    if (s == "DamageOverTime") return StatusEffectType::DamageOverTime;
    if (s == "HealOverTime") return StatusEffectType::HealOverTime;
    if (s == "Shield") return StatusEffectType::Shield;
    if (s == "CrowdControl") return StatusEffectType::CrowdControl;
    throw std::runtime_error("Unknown StatusEffectType: " + std::string(s));
}

static CrowdControlType parseCrowdControlType(std::string_view s)
{
    if (s == "None") return CrowdControlType::None;
    if (s == "Stun") return CrowdControlType::Stun;
    if (s == "Knockup") return CrowdControlType::Knockup;
    if (s == "Root") return CrowdControlType::Root;
    if (s == "Silence") return CrowdControlType::Silence;
    if (s == "Disarm") return CrowdControlType::Disarm;
    if (s == "Taunt") return CrowdControlType::Taunt;
    if (s == "Fear") return CrowdControlType::Fear;
    if (s == "Suppression") return CrowdControlType::Suppression;
    throw std::runtime_error("Unknown CrowdControlType: " + std::string(s));
}

static StatType parseStatType(std::string_view s)
{
    if (s == "AttackDamage") return StatType::AttackDamage;
    if (s == "AbilityPower") return StatType::AbilityPower;
    if (s == "Armor") return StatType::Armor;
    if (s == "MagicResist") return StatType::MagicResist;
    if (s == "AttackSpeed") return StatType::AttackSpeed;
    if (s == "CritChance") return StatType::CritChance;
    if (s == "CritDamage") return StatType::CritDamage;
    if (s == "MaxHp") return StatType::MaxHp;
    if (s == "Omnivamp") return StatType::Omnivamp;
    if (s == "DamageAmplification") return StatType::DamageAmplification;
    if (s == "ManaGainOnAttack") return StatType::ManaGainOnAttack;
    if (s == "HealingDone") return StatType::HealingDone;
    if (s == "DamageReduction") return StatType::DamageReduction;
    if (s == "None") return StatType::None;
    throw std::runtime_error("Unknown StatType: " + std::string(s));
}

static ModifierType parseModifierType(std::string_view s)
{
    if (s == "Flat") return ModifierType::Flat;
    if (s == "Percent") return ModifierType::Percent;
    throw std::runtime_error("Unknown ModifierType: " + std::string(s));
}

static TraitHook parseTraitHook(std::string_view s)
{
    if (s == "OnCombatStart") return TraitHook::OnCombatStart;
    if (s == "OnAttack") return TraitHook::OnAttack;
    if (s == "OnHit") return TraitHook::OnHit;
    if (s == "OnKill") return TraitHook::OnKill;
    if (s == "OnCast") return TraitHook::OnCast;
    if (s == "OnCrit") return TraitHook::OnCrit;
    if (s == "OnLowHealth") return TraitHook::OnLowHealth;
    if (s == "AfterDamage") return TraitHook::AfterDamage;
    if (s == "Periodic") return TraitHook::Periodic;
    if (s == "AuraUpdate") return TraitHook::AuraUpdate;
    throw std::runtime_error("Unknown TraitHook: " + std::string(s));
}

static TraitEffectType parseTraitEffectType(std::string_view s)
{
    if (s == "ApplyStatusToTraitUnits") return TraitEffectType::ApplyStatusToTraitUnits;
    if (s == "ApplyStatusToEnemyTeam") return TraitEffectType::ApplyStatusToEnemyTeam;
    if (s == "StackStatusOnAttack") return TraitEffectType::StackStatusOnAttack;
    if (s == "ShieldOnCombatStart") return TraitEffectType::ShieldOnCombatStart;
    if (s == "ExecuteBelowHpPercent") return TraitEffectType::ExecuteBelowHpPercent;
    if (s == "TempCritBonusVsLowHp") return TraitEffectType::TempCritBonusVsLowHp;
    throw std::runtime_error("Unknown TraitEffectType: " + std::string(s));
}

static StatusEffect parseStatusEffect(const JsonValue& obj)
{
    StatusEffect e{};
    e.name = requiredString(obj, "name");
    e.effectType = parseStatusEffectType(requiredString(obj, "effectType"));
    e.crowdControlType = parseCrowdControlType(hasKey(obj, "crowdControlType") ? requiredString(obj, "crowdControlType") : "None");
    e.affectedStat = parseStatType(hasKey(obj, "affectedStat") ? requiredString(obj, "affectedStat") : "None");
    e.modifierType = parseModifierType(hasKey(obj, "modifierType") ? requiredString(obj, "modifierType") : "Flat");
    e.value = requiredFloat(obj, "value");
    e.durationMs = optionalInt(obj, "durationMs", 0);
    e.remainingMs = optionalInt(obj, "remainingMs", 0);
    e.tickIntervalMs = optionalInt(obj, "tickIntervalMs", 0);
    e.tickTimerMs = optionalInt(obj, "tickTimerMs", 0);
    e.damageType = parseDamageType(hasKey(obj, "damageType") ? requiredString(obj, "damageType") : "True");
    return e;
}

static DamageFormula parseDamageFormula(const JsonValue& obj)
{
    DamageFormula f{};
    f.baseDamage = optionalInt(obj, "baseDamage", 0);
    f.adRatio = optionalFloat(obj, "adRatio", 0.0f);
    f.apRatio = optionalFloat(obj, "apRatio", 0.0f);
    f.damageType = parseDamageType(hasKey(obj, "damageType") ? requiredString(obj, "damageType") : "Physical");
    return f;
}

static AbilityEffect parseAbilityEffect(const JsonValue& obj)
{
    AbilityEffect e{};
    e.name = hasKey(obj, "name") ? requiredString(obj, "name") : "";
    e.trigger = parseAbilityTrigger(requiredString(obj, "trigger"));

    if (hasKey(obj, "damageFormula"))
    {
        e.damageFormula = parseDamageFormula(obj.at("damageFormula"));
    }

    e.healAmount = optionalInt(obj, "healAmount", 0);
    e.healPercentOfDamage = optionalFloat(obj, "healPercentOfDamage", 0.0f);
    e.shieldAmount = optionalInt(obj, "shieldAmount", 0);
    e.maxStacks = optionalInt(obj, "maxStacks", 0);
    e.targetMaxHpThreshold = optionalInt(obj, "targetMaxHpThreshold", 0);
    e.targetMaxHpPercentDamage = optionalFloat(obj, "targetMaxHpPercentDamage", 0.0f);

    e.areaShape = parseAreaShape(hasKey(obj, "areaShape") ? requiredString(obj, "areaShape") : "SingleTarget");
    e.radius = optionalInt(obj, "radius", 0);
    e.delayMs = optionalInt(obj, "delayMs", 0);

    e.appliesStatusEffect = optionalBool(obj, "appliesStatusEffect", false);
    if (e.appliesStatusEffect && hasKey(obj, "appliedStatusEffect"))
    {
        e.appliedStatusEffect = parseStatusEffect(obj.at("appliedStatusEffect"));
    }

    e.canCrit = optionalBool(obj, "canCrit", false);
    e.critChanceOverride = optionalFloat(obj, "critChanceOverride", CombatConstants::NoOverrideFloat);
    e.critDamageOverride = optionalFloat(obj, "critDamageOverride", CombatConstants::NoOverrideFloat);
    return e;
}

static Ability parseAbility(const JsonValue& root)
{
    Ability a{};
    a.name = requiredString(root, "name");
    a.manaCost = optionalInt(root, "manaCost", 0);
    a.targetType = parseTargetType(hasKey(root, "targetType") ? requiredString(root, "targetType") : "CurrentEnemy");

    if (hasKey(root, "effects"))
    {
        const JsonValue& effects = root.at("effects");
        if (!effects.isArray())
        {
            throw std::runtime_error("Ability.effects must be array");
        }
        for (const JsonValue& e : effects.asArray())
        {
            a.effects.push_back(parseAbilityEffect(e));
        }
    }
    return a;
}

static Item parseItem(const JsonValue& root)
{
    Item item{};
    item.name = requiredString(root, "name");

    if (hasKey(root, "passiveStats"))
    {
        const JsonValue& arr = root.at("passiveStats");
        if (!arr.isArray())
        {
            throw std::runtime_error("Item.passiveStats must be array");
        }
        for (const JsonValue& e : arr.asArray())
        {
            item.passiveStats.push_back(parseStatusEffect(e));
        }
    }

    if (hasKey(root, "triggeredEffects"))
    {
        const JsonValue& arr = root.at("triggeredEffects");
        if (!arr.isArray())
        {
            throw std::runtime_error("Item.triggeredEffects must be array");
        }
        for (const JsonValue& e : arr.asArray())
        {
            item.triggeredEffects.push_back(parseAbilityEffect(e));
        }
    }

    return item;
}

static TraitDefinition parseTrait(const JsonValue& root)
{
    TraitDefinition def{};
    def.trait.name = requiredString(root, "name");

    const JsonValue& bps = root.at("breakpoints");
    if (!bps.isArray())
    {
        throw std::runtime_error("Trait.breakpoints must be array");
    }
    for (const JsonValue& v : bps.asArray())
    {
        def.trait.breakpoints.push_back(static_cast<int>(std::lround(v.asNumber())));
    }

    if (hasKey(root, "tiers"))
    {
        const JsonValue& tiers = root.at("tiers");
        if (!tiers.isArray())
        {
            throw std::runtime_error("Trait.tiers must be array");
        }
        for (const JsonValue& t : tiers.asArray())
        {
            TraitTier tier{};
            tier.breakpoint = requiredInt(t, "breakpoint");

            const JsonValue& eff = t.at("effects");
            if (!eff.isArray())
            {
                throw std::runtime_error("TraitTier.effects must be array");
            }
            for (const JsonValue& e : eff.asArray())
            {
                TraitEffect te{};
                te.hook = parseTraitHook(requiredString(e, "hook"));
                te.type = parseTraitEffectType(requiredString(e, "type"));
                if (hasKey(e, "statusEffect"))
                {
                    te.statusEffect = parseStatusEffect(e.at("statusEffect"));
                }
                te.shieldAmount = optionalInt(e, "shieldAmount", 0);
                te.targetHpPercentThreshold = optionalFloat(e, "targetHpPercentThreshold", -1.0f);
                te.maxStacks = optionalInt(e, "maxStacks", 0);
                te.critChanceBonus = optionalFloat(e, "critChanceBonus", 0.0f);
                te.critDamageBonus = optionalFloat(e, "critDamageBonus", 0.0f);
                te.periodMs = optionalInt(e, "periodMs", 0);
                te.auraRefreshMs = optionalInt(e, "auraRefreshMs", 0);
                te.isAura = optionalBool(e, "isAura", false);
                tier.effects.push_back(std::move(te));
            }

            def.tiers.push_back(std::move(tier));
        }
    }

    return def;
}

static ChampionDefinition parseChampion(const JsonValue& root)
{
    ChampionDefinition c{};
    c.name = trimEdges(requiredString(root, "name"));
    c.cost = optionalInt(root, "cost", 1);
    c.spritePath = trimEdges(optionalString(root, "spritePath", ""));

    if (hasKey(root, "stats"))
    {
        const JsonValue& stats = root.at("stats");
        c.hp = requiredInt(stats, "hp");
        c.ad = requiredInt(stats, "ad");
        c.armor = requiredInt(stats, "armor");
        c.magicResist = requiredInt(stats, "mr");
        c.abilityPower = optionalFloat(stats, "ap", 0.0f);
        c.attackSpeed = optionalFloat(stats, "attackSpeed", 1.0f);
        c.critChance = optionalFloat(stats, "critChance", 0.0f);
        c.critDamage = optionalFloat(stats, "critDamage", CombatConstants::DefaultCritDamageMultiplier);
        c.durability = optionalFloat(stats, "durability", 0.0f);
        c.range = optionalInt(stats, "range", 1);
    }
    else
    {
        c.hp = requiredInt(root, "hp");
        c.ad = requiredInt(root, "ad");
        c.armor = requiredInt(root, "armor");
        c.magicResist = requiredInt(root, "magicResist");
        c.abilityPower = optionalFloat(root, "abilityPower", 0.0f);
        c.attackSpeed = optionalFloat(root, "attackSpeed", 1.0f);
        c.critChance = optionalFloat(root, "critChance", 0.0f);
        c.critDamage = optionalFloat(root, "critDamage", CombatConstants::DefaultCritDamageMultiplier);
        c.durability = optionalFloat(root, "durability", 0.0f);
        c.range = optionalInt(root, "range", 1);
    }

    c.autoAttackDamageType = parseDamageType(optionalString(root, "autoAttackDamageType", "Physical"));

    if (hasKey(root, "mana"))
    {
        const JsonValue& mana = root.at("mana");
        c.maxMana = optionalInt(mana, "maxMana", 0);
        c.manaGainOnAttack = optionalInt(mana, "manaGainOnAttack", 0);
        c.startMana = optionalInt(mana, "startMana", 0);
    }
    else
    {
        c.maxMana = optionalInt(root, "maxMana", 0);
        c.startMana = optionalInt(root, "mana", 0);
        c.manaGainOnAttack = optionalInt(root, "manaGainOnAttack", 0);
    }

    if (hasKey(root, "traits"))
    {
        const JsonValue& arr = root.at("traits");
        if (!arr.isArray())
        {
            throw std::runtime_error("Champion.traits must be array");
        }
        for (const JsonValue& t : arr.asArray())
        {
            if (t.isString())
            {
                c.traits.push_back(trimEdges(t.asString()));
            }
        }
    }

    c.isPlayable = optionalBool(root, "isPlayable", true);
    if (hasKey(root, "tags"))
    {
        const JsonValue& arr = root.at("tags");
        if (!arr.isArray())
        {
            throw std::runtime_error("Champion.tags must be array");
        }
        for (const JsonValue& t : arr.asArray())
        {
            if (t.isString())
            {
                c.tags.push_back(trimEdges(t.asString()));
            }
        }
    }

    if (hasKey(root, "abilityId"))
    {
        c.abilityId = requiredString(root, "abilityId");
    }
    else if (hasKey(root, "abilityName"))
    {
        c.abilityId = requiredString(root, "abilityName");
    }
    else
    {
        c.abilityId = "";
    }
    c.abilityId = trimEdges(c.abilityId);
    return c;
}

static std::vector<std::filesystem::path> listJsonFiles(const std::filesystem::path& dir)
{
    std::vector<std::filesystem::path> files;
    if (!std::filesystem::exists(dir))
    {
        return files;
    }
    for (const auto& entry : std::filesystem::directory_iterator(dir))
    {
        if (!entry.is_regular_file())
        {
            continue;
        }
        const auto p = entry.path();
        if (p.extension() == ".json")
        {
            files.push_back(p);
        }
    }
    std::sort(files.begin(), files.end());
    return files;
}

bool ContentManager::loadAll(const std::string& dataRootDir)
{
    abilities_.clear();
    traits_.clear();
    items_.clear();
    champions_.clear();

    const std::filesystem::path root = std::filesystem::path(dataRootDir);
    const std::filesystem::path abilitiesDir = root / "abilities";
    const std::filesystem::path traitsDir = root / "traits";
    const std::filesystem::path itemsDir = root / "items";
    const std::filesystem::path championsDir = root / "champions";

    for (const auto& path : listJsonFiles(abilitiesDir))
    {
        const std::string s = readFileToString(path);
        const JsonValue j = parseJson(s);
        const std::string id = requiredString(j, "id");
        abilities_.emplace(id, parseAbility(j));
        std::cout << "Loaded ability: " << id << "\n";
    }

    for (const auto& path : listJsonFiles(traitsDir))
    {
        const std::string s = readFileToString(path);
        const JsonValue j = parseJson(s);
        TraitDefinition def = parseTrait(j);
        traits_.emplace(def.trait.name, std::move(def));
    }

    for (const auto& path : listJsonFiles(itemsDir))
    {
        const std::string s = readFileToString(path);
        const JsonValue j = parseJson(s);
        Item item = parseItem(j);
        items_.emplace(item.name, std::move(item));
    }

    for (const auto& path : listJsonFiles(championsDir))
    {
        const std::string s = readFileToString(path);
        const JsonValue j = parseJson(s);
        ChampionDefinition c = parseChampion(j);
        const std::string name = c.name;
        champions_.emplace(name, std::move(c));
        std::cout << "Loaded champion: " << name << "\n";
    }

    for (const auto& [name, champ] : champions_)
    {
        if (!champ.abilityId.empty() && abilities_.find(champ.abilityId) == abilities_.end())
        {
            throw std::runtime_error("Champion references missing abilityId: " + champ.name + " -> " + champ.abilityId);
        }
        if (!traits_.empty())
        {
            for (const std::string& t : champ.traits)
            {
                if (traits_.find(t) == traits_.end())
                {
                    throw std::runtime_error("Champion references missing trait: " + champ.name + " -> " + t);
                }
            }
        }
    }

    return true;
}

std::size_t ContentManager::championCount() const { return champions_.size(); }
std::size_t ContentManager::traitCount() const { return traits_.size(); }
std::size_t ContentManager::itemCount() const { return items_.size(); }
std::size_t ContentManager::abilityCount() const { return abilities_.size(); }

const ChampionDefinition* ContentManager::getChampion(std::string_view name) const
{
    auto it = champions_.find(std::string(name));
    if (it == champions_.end()) return nullptr;
    return &it->second;
}

const Ability* ContentManager::getAbility(std::string_view id) const
{
    auto it = abilities_.find(std::string(id));
    if (it == abilities_.end()) return nullptr;
    return &it->second;
}

const TraitDefinition* ContentManager::getTrait(std::string_view name) const
{
    auto it = traits_.find(std::string(name));
    if (it == traits_.end()) return nullptr;
    return &it->second;
}

const Item* ContentManager::getItem(std::string_view name) const
{
    auto it = items_.find(std::string(name));
    if (it == items_.end()) return nullptr;
    return &it->second;
}

std::vector<std::string> getLoadedChampionNames(const ContentManager& content)
{
    std::vector<std::string> names;
    names.reserve(content.champions().size());
    for (const auto& [name, _] : content.champions())
    {
        names.push_back(name);
    }
    std::sort(names.begin(), names.end());
    return names;
}

std::vector<std::string> getPlayableChampionNames(const ContentManager& content)
{
    std::vector<std::string> names;
    names.reserve(content.champions().size());
    for (const auto& [name, champ] : content.champions())
    {
        if (isPlayableChampion(champ))
        {
            names.push_back(name);
        }
    }
    std::sort(names.begin(), names.end());
    return names;
}

std::string pickChampionByIndex(const ContentManager& content, std::size_t index)
{
    const std::vector<std::string> names = getPlayableChampionNames(content);
    if (names.empty())
    {
        throw std::runtime_error("No playable champions loaded from data root");
    }
    return names[index % names.size()];
}

static Position defaultTeamPos(TeamId team, std::size_t i)
{
    const int ix = static_cast<int>(i);
    if (team == TeamId::TeamA)
    {
        return Position{ 4 + (ix % 3), 7 + (ix / 3) };
    }
    return Position{ 4 + (ix % 3), 2 - (ix / 3) };
}

std::vector<Unit> makeDynamicTeam(const ContentManager& content,
                                 TeamId team,
                                 std::size_t count,
                                 std::size_t startIndex,
                                 const std::vector<Position>& positions)
{
    if (content.champions().empty())
    {
        throw std::runtime_error("No champions loaded from data root");
    }

    std::vector<Unit> out;
    out.reserve(count);
    for (std::size_t i = 0; i < count; ++i)
    {
        const std::string name = pickChampionByIndex(content, startIndex + i);
        const Position pos = i < positions.size() ? positions[i] : defaultTeamPos(team, i);
        out.push_back(content.createUnit(name, pos, team));
    }
    return out;
}

Unit ContentManager::createUnit(std::string_view championName,
                                const Position& position,
                                TeamId team) const
{
    return UnitFactory(*this).createFromChampion(championName, position, team);
}

const std::unordered_map<std::string, TraitDefinition>& ContentManager::traits() const
{
    return traits_;
}

const std::unordered_map<std::string, Item>& ContentManager::items() const
{
    return items_;
}

const std::unordered_map<std::string, Ability>& ContentManager::abilities() const
{
    return abilities_;
}

const std::unordered_map<std::string, ChampionDefinition>& ContentManager::champions() const
{
    return champions_;
}
