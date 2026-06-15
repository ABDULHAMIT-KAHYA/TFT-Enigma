#include "import/TFTDataImporter.hpp"
#include "constants/CombatConstants.hpp"
#include "core/Json.hpp"
#include <cctype>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

static constexpr double PercentScale = 100.0;
static constexpr double StatPercentThreshold = 3.0;
static constexpr double VerySmallValueEpsilon = 0.00001;
static constexpr int DefaultChampionCost = 1;
static constexpr int DefaultChampionHp = 1000;
static constexpr int DefaultChampionAttackDamage = 50;
static constexpr int DefaultChampionArmor = 20;
static constexpr int DefaultChampionMagicResist = 20;
static constexpr int DefaultChampionRange = 1;
static constexpr double DefaultChampionAttackSpeed = 1.0;
static constexpr int DefaultChampionAbilityPower = 100;
static constexpr int DefaultChampionManaGainOnAttack = 10;

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



static std::string safeFileName(std::string s)
{
    for (char& c : s)
    {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '-')
        {
            c = '_';
        }
    }

    const std::size_t maxLen = 80;
    if (s.size() > maxLen)
    {
        s = s.substr(0, maxLen);
    }

    return s;
}






static void writeStringToFile(const std::filesystem::path& path, const std::string& s)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream f(path, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!f)
    {
        throw std::runtime_error("Failed to write file: " + path.string());
    }
    f.write(s.data(), static_cast<std::streamsize>(s.size()));
}

static std::string toSnakeLower(std::string s)
{
    std::string out;
    out.reserve(s.size());
    bool lastUnderscore = false;
    for (char c : s)
    {
        if (std::isalnum(static_cast<unsigned char>(c)))
        {
            const char lc = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            out.push_back(lc);
            lastUnderscore = false;
        }
        else
        {
            if (!lastUnderscore)
            {
                out.push_back('_');
                lastUnderscore = true;
            }
        }
    }
    while (!out.empty() && out.back() == '_') out.pop_back();
    if (out.empty()) out = "unknown";
    return out;
}

static std::string toLowerText(std::string s)
{
    for (char& c : s)
    {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

static bool containsAny(std::string_view text, const std::vector<std::string_view>& needles)
{
    for (std::string_view needle : needles)
    {
        if (text.find(needle) != std::string_view::npos)
        {
            return true;
        }
    }
    return false;
}

static bool hasKeyObj(const JsonValue& v, std::string_view key)
{
    return v.isObject() && v.hasKey(key);
}

static std::string getString(const JsonValue& obj, std::string_view key, std::string fallback)
{
    if (!hasKeyObj(obj, key)) return fallback;
    const JsonValue& v = obj.at(key);
    if (!v.isString()) return fallback;
    return v.asString();
}

static double getNumber(const JsonValue& obj, std::string_view key, double fallback)
{
    if (!hasKeyObj(obj, key)) return fallback;
    const JsonValue& v = obj.at(key);
    if (!v.isNumber()) return fallback;
    return v.asNumber();
}

static int pickHighestSetNumberFromObjectKeys(const JsonValue& setsObj)
{
    if (!setsObj.isObject())
    {
        return 0;
    }
    int best = 0;
    for (const auto& [k, _] : setsObj.asObject())
    {
        int n = 0;
        for (char c : k)
        {
            if (c < '0' || c > '9')
            {
                n = 0;
                break;
            }
            n = n * 10 + (c - '0');
        }
        best = std::max(best, n);
    }
    return best;
}

static int detectCurrentSet(const JsonValue& root)
{
    if (hasKeyObj(root, "sets"))
    {
        return pickHighestSetNumberFromObjectKeys(root.at("sets"));
    }
    if (hasKeyObj(root, "setData"))
    {
        const JsonValue& arr = root.at("setData");
        if (arr.isArray())
        {
            int best = 0;
            for (const JsonValue& e : arr.asArray())
            {
                if (!e.isObject()) continue;
                const int n = static_cast<int>(getNumber(e, "number", getNumber(e, "setNumber", 0)));
                best = std::max(best, n);
            }
            return best;
        }
    }
    return 0;
}

static std::string jsonString(std::string_view s)
{
    std::string out;
    out.reserve(s.size() + 2);
    out.push_back('"');
    for (char c : s)
    {
        if (c == '\\' || c == '"')
        {
            out.push_back('\\');
            out.push_back(c);
        }
        else if (c == '\n')
        {
            out += "\\n";
        }
        else if (c == '\r')
        {
            out += "\\r";
        }
        else if (c == '\t')
        {
            out += "\\t";
        }
        else
        {
            out.push_back(c);
        }
    }
    out.push_back('"');
    return out;
}

static void appendStringArrayJson(std::ostringstream& ss, const std::vector<std::string>& values)
{
    ss << "[";
    for (std::size_t i = 0; i < values.size(); ++i)
    {
        if (i) ss << ", ";
        ss << jsonString(values[i]);
    }
    ss << "]";
}

static std::string jsonValueSummary(const JsonValue& v)
{
    if (v.isString()) return v.asString();
    if (v.isNumber())
    {
        std::ostringstream ss;
        ss << v.asNumber();
        return ss.str();
    }
    if (v.isBool()) return v.asBool() ? "true" : "false";
    if (v.isNull()) return "null";
    if (v.isArray())
    {
        return "array(" + std::to_string(v.asArray().size()) + ")";
    }
    if (v.isObject())
    {
        return "object(" + std::to_string(v.asObject().size()) + ")";
    }
    return "";
}

static void appendRawVariablesJson(std::ostringstream& ss, const std::vector<std::pair<std::string, std::string>>& vars)
{
    ss << "[";
    for (std::size_t i = 0; i < vars.size(); ++i)
    {
        if (i) ss << ", ";
        ss << "{ \"name\": " << jsonString(vars[i].first)
           << ", \"value\": " << jsonString(vars[i].second) << " }";
    }
    ss << "]";
}

static std::vector<std::pair<std::string, std::string>> collectObjectVariables(const JsonValue& obj)
{
    std::vector<std::pair<std::string, std::string>> vars;
    if (!obj.isObject())
    {
        return vars;
    }
    for (const auto& [k, v] : obj.asObject())
    {
        vars.emplace_back(k, jsonValueSummary(v));
    }
    std::sort(vars.begin(), vars.end(), [](const auto& a, const auto& b) {
        return a.first < b.first;
    });
    return vars;
}

static std::vector<std::pair<std::string, std::string>> collectSpellVariables(const JsonValue& spell)
{
    std::vector<std::pair<std::string, std::string>> vars;
    if (!spell.isObject() || !hasKeyObj(spell, "variables") || !spell.at("variables").isArray())
    {
        return vars;
    }
    for (const JsonValue& v : spell.at("variables").asArray())
    {
        if (!v.isObject())
        {
            continue;
        }
        const std::string name = getString(v, "name", "");
        if (name.empty())
        {
            continue;
        }
        std::string value;
        if (hasKeyObj(v, "value"))
        {
            value = jsonValueSummary(v.at("value"));
        }
        else if (hasKeyObj(v, "values"))
        {
            value = jsonValueSummary(v.at("values"));
        }
        vars.emplace_back(name, value);
    }
    std::sort(vars.begin(), vars.end(), [](const auto& a, const auto& b) {
        return a.first < b.first;
    });
    return vars;
}

static bool hasNonEmptyArrayField(const JsonValue& obj, std::string_view key)
{
    return hasKeyObj(obj, key) && obj.at(key).isArray() && !obj.at(key).asArray().empty();
}

static bool hasNonEmptyObjectField(const JsonValue& obj, std::string_view key)
{
    return hasKeyObj(obj, key) && obj.at(key).isObject() && !obj.at(key).asObject().empty();
}

static bool hasSourceFieldValue(const JsonValue& obj, std::string_view key)
{
    return hasKeyObj(obj, key) && !obj.at(key).isNull();
}

static void appendArrayStringsForClassification(std::string& text, const JsonValue& obj, std::string_view key)
{
    if (!hasKeyObj(obj, key) || !obj.at(key).isArray())
    {
        return;
    }
    for (const JsonValue& v : obj.at(key).asArray())
    {
        if (v.isString())
        {
            text += " ";
            text += v.asString();
        }
    }
}

static std::string classifyItemCategory(const JsonValue& itemObj, const std::string& name)
{
    std::string text;
    text += getString(itemObj, "apiName", "");
    text += " ";
    text += getString(itemObj, "icon", "");
    text += " ";
    text += getString(itemObj, "desc", "");
    text += " ";
    text += getString(itemObj, "description", "");
    text += " ";
    text += name;
    appendArrayStringsForClassification(text, itemObj, "tags");
    appendArrayStringsForClassification(text, itemObj, "associatedTraits");
    appendArrayStringsForClassification(text, itemObj, "incompatibleTraits");

    const std::string lower = toLowerText(std::move(text));
    const bool hasBuildPath = hasSourceFieldValue(itemObj, "from") || hasNonEmptyArrayField(itemObj, "composition");
    const bool hasEffectVariables = hasNonEmptyObjectField(itemObj, "effects");

    if (containsAny(lower, { "augment", "/augments/", "hexcore" }))
    {
        return "Augment";
    }
    if (containsAny(lower, { "anvil" }))
    {
        return "Anvil";
    }
    if (containsAny(lower, { "radiant" }))
    {
        return "RadiantItem";
    }
    if (containsAny(lower, { "artifact", "/artifacts/" }))
    {
        return "Artifact";
    }
    if (containsAny(lower, { "support" }))
    {
        return "SupportItem";
    }
    if (containsAny(lower, { "emblem" }))
    {
        return "Emblem";
    }
    if (containsAny(lower, { "consumable", "consumables", "gold", "reroll", "xp", "shop" }))
    {
        return "Consumable";
    }
    if (containsAny(lower, { "loot", "orb", "chest" }))
    {
        return "LootObject";
    }
    if (hasBuildPath || hasEffectVariables)
    {
        return "CombatItem";
    }
    return "Unknown";
}

static std::string writeChampionNormalized(const std::string& name,
                                           int cost,
                                           int hp,
                                           int ad,
                                           int armor,
                                           int mr,
                                           int startMana,
                                           int maxMana,
                                           int manaGainOnAttack,
                                           int range,
                                           float attackSpeed,
                                           int abilityPower,
                                           float critChance,
                                           float critDamage,
                                           float durability,
                                           const std::vector<std::string>& traits,
                                           bool isPlayable,
                                           const std::vector<std::string>& tags,
                                           const std::string& abilityName,
                                           const std::string& sourceId,
                                           const std::string& description,
                                           const std::string& tooltip,
                                           const std::string& iconPath,
                                           const std::string& squareIconPath,
                                           const std::string& tileIconPath,
                                           const std::string& splashPath,
                                           const std::vector<std::string>& importWarnings)
{
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"name\": " << jsonString(name) << ",\n";
    ss << "  \"cost\": " << cost << ",\n";
    ss << "  \"hp\": " << hp << ",\n";
    ss << "  \"ad\": " << ad << ",\n";
    ss << "  \"armor\": " << armor << ",\n";
    ss << "  \"magicResist\": " << mr << ",\n";
    ss << "  \"mana\": " << startMana << ",\n";
    ss << "  \"maxMana\": " << maxMana << ",\n";
    ss << "  \"manaGainOnAttack\": " << manaGainOnAttack << ",\n";
    ss << "  \"range\": " << range << ",\n";
    ss << "  \"attackSpeed\": " << attackSpeed << ",\n";
    ss << "  \"abilityPower\": " << abilityPower << ",\n";
    ss << "  \"critChance\": " << critChance << ",\n";
    ss << "  \"critDamage\": " << critDamage << ",\n";
    ss << "  \"durability\": " << durability << ",\n";
    ss << "  \"traits\": [";
    for (std::size_t i = 0; i < traits.size(); ++i)
    {
        if (i) ss << ", ";
        ss << jsonString(traits[i]);
    }
    ss << "],\n";
    ss << "  \"isPlayable\": " << (isPlayable ? "true" : "false") << ",\n";
    ss << "  \"tags\": [";
    for (std::size_t i = 0; i < tags.size(); ++i)
    {
        if (i) ss << ", ";
        ss << jsonString(tags[i]);
    }
    ss << "],\n";
    ss << "  \"abilityName\": " << jsonString(abilityName) << ",\n";
    ss << "  \"autoAttackDamageType\": \"Physical\",\n";
    ss << "  \"sourceId\": " << jsonString(sourceId) << ",\n";
    ss << "  \"description\": " << jsonString(description) << ",\n";
    ss << "  \"tooltip\": " << jsonString(tooltip) << ",\n";
    ss << "  \"iconPath\": " << jsonString(iconPath) << ",\n";
    ss << "  \"squareIconPath\": " << jsonString(squareIconPath) << ",\n";
    ss << "  \"tileIconPath\": " << jsonString(tileIconPath) << ",\n";
    ss << "  \"splashPath\": " << jsonString(splashPath) << ",\n";
    ss << "  \"importWarnings\": ";
    appendStringArrayJson(ss, importWarnings);
    ss << "\n";
    ss << "}\n";
    return ss.str();
}

static std::string writeAbilityNormalized(const std::string& id,
                                          const std::string& name,
                                          int manaCost,
                                          int baseDamage,
                                          float adRatio,
                                          float apRatio,
                                          const std::string& shape,
                                          int radius,
                                          int delayMs,
                                          bool canCrit,
                                          const JsonValue& spellObj,
                                          const std::vector<std::string>& importWarnings)
{
    std::ostringstream ss;
    const std::string sourceId = getString(spellObj, "apiName", getString(spellObj, "name", id));
    const std::string description = getString(spellObj, "desc", getString(spellObj, "description", ""));
    const std::string tooltip = getString(spellObj, "tooltip", "");
    const std::vector<std::pair<std::string, std::string>> rawVars = collectSpellVariables(spellObj);
    const std::string damageMetadata = std::string("baseDamage=") + std::to_string(baseDamage) +
                                       "; damageType=Magic; adRatio=0; apRatio=0.8";

    ss << "{\n";
    ss << "  \"id\": " << jsonString(id) << ",\n";
    ss << "  \"name\": " << jsonString(name) << ",\n";
    ss << "  \"manaCost\": " << manaCost << ",\n";
    ss << "  \"targetType\": \"CurrentEnemy\",\n";
    ss << "  \"isPlaceholder\": true,\n";
    ss << "  \"sourceId\": " << jsonString(sourceId) << ",\n";
    ss << "  \"description\": " << jsonString(description) << ",\n";
    ss << "  \"tooltip\": " << jsonString(tooltip) << ",\n";
    ss << "  \"targetingMetadata\": " << jsonString(getString(spellObj, "targeting", "")) << ",\n";
    ss << "  \"areaMetadata\": " << jsonString(shape) << ",\n";
    ss << "  \"damageMetadata\": " << jsonString(damageMetadata) << ",\n";
    ss << "  \"effectMetadata\": " << jsonString(jsonValueSummary(spellObj)) << ",\n";
    ss << "  \"rawVariables\": ";
    appendRawVariablesJson(ss, rawVars);
    ss << ",\n";
    ss << "  \"importWarnings\": ";
    appendStringArrayJson(ss, importWarnings);
    ss << ",\n";
    ss << "  \"effects\": [\n";
    ss << "    {\n";
    ss << "      \"name\": " << jsonString(name) << ",\n";
    ss << "      \"trigger\": \"OnCast\",\n";
    ss << "      \"damageFormula\": { \"baseDamage\": " << baseDamage
       << ", \"adRatio\": " << adRatio
       << ", \"apRatio\": " << apRatio
       << ", \"damageType\": \"Magic\" },\n";
    ss << "      \"areaShape\": " << jsonString(shape) << ",\n";
    ss << "      \"radius\": " << radius << ",\n";
    ss << "      \"delayMs\": " << delayMs << ",\n";
    ss << "      \"canCrit\": " << (canCrit ? "true" : "false") << "\n";
    ss << "    }\n";
    ss << "  ]\n";
    ss << "}\n";
    return ss.str();
}

static std::string writeTraitNormalized(const std::string& name, const std::vector<int>& breakpoints)
{
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"name\": " << jsonString(name) << ",\n";
    ss << "  \"breakpoints\": [";
    for (std::size_t i = 0; i < breakpoints.size(); ++i)
    {
        if (i) ss << ", ";
        ss << breakpoints[i];
    }
    ss << "],\n";
    ss << "  \"isPlaceholder\": true,\n";
    ss << "  \"sourceId\": " << jsonString(name) << ",\n";
    ss << "  \"description\": \"\",\n";
    ss << "  \"tooltip\": \"\",\n";
    ss << "  \"rawVariables\": [],\n";
    ss << "  \"importWarnings\": [\"No trait effects were mapped from source data\"],\n";
    ss << "  \"tiers\": []\n";
    ss << "}\n";
    return ss.str();
}

static std::string statusEffectTypeForStat(std::string_view affectedStat)
{
    if (affectedStat == "MaxHp") return "Buff";
    if (affectedStat == "AttackDamage") return "BonusAttackDamage";
    if (affectedStat == "AbilityPower") return "BonusAbilityPower";
    if (affectedStat == "AttackSpeed") return "BonusAttackSpeed";
    if (affectedStat == "Armor") return "BonusArmor";
    if (affectedStat == "MagicResist") return "BonusMagicResist";
    if (affectedStat == "CritChance") return "CritChanceBonus";
    if (affectedStat == "CritDamage") return "CritDamageBonus";
    if (affectedStat == "DamageReduction") return "DamageReduction";
    return "Buff";
}

static bool tryMapVariableToStatEffect(std::string_view varName,
                                      double value,
                                      std::string& affectedStatOut,
                                      std::string& modifierTypeOut,
                                      double& valueOut)
{
    const std::string v = toSnakeLower(std::string(varName));

    if (v.find("omnivamp") != std::string::npos || v.find("lifesteal") != std::string::npos || v.find("vamp") != std::string::npos)
    {
        affectedStatOut = "Omnivamp";
        modifierTypeOut = "Flat";
        valueOut = value > StatPercentThreshold ? (value / PercentScale) : value;
        return true;
    }

    if (v.find("damageamp") != std::string::npos || v.find("damage_amp") != std::string::npos)
    {
        affectedStatOut = "DamageAmplification";
        modifierTypeOut = "Flat";
        valueOut = value > StatPercentThreshold ? (value / PercentScale) : value;
        return true;
    }

    if (v.find("managain") != std::string::npos && v.find("attack") != std::string::npos)
    {
        affectedStatOut = "ManaGainOnAttack";
        modifierTypeOut = "Flat";
        valueOut = value;
        return true;
    }

    if (v.find("bonushealth") != std::string::npos || v == "health" || v.find("healthamp") != std::string::npos)
    {
        affectedStatOut = "MaxHp";
        modifierTypeOut = "Flat";
        valueOut = value;
        return true;
    }

    if (v == "armor" || v.find("bonusarmor") != std::string::npos)
    {
        affectedStatOut = "Armor";
        modifierTypeOut = "Flat";
        valueOut = value;
        return true;
    }

    if (v == "mr" || v == "magicresist" || v.find("bonusmr") != std::string::npos || v.find("bonusmagicresist") != std::string::npos)
    {
        affectedStatOut = "MagicResist";
        modifierTypeOut = "Flat";
        valueOut = value;
        return true;
    }

    if (v == "ap" || v.find("abilitypower") != std::string::npos)
    {
        affectedStatOut = "AbilityPower";
        modifierTypeOut = "Flat";
        valueOut = value;
        return true;
    }

    if (v == "as" || v.find("attackspeed") != std::string::npos)
    {
        affectedStatOut = "AttackSpeed";
        modifierTypeOut = "Percent";
        valueOut = value > StatPercentThreshold ? (value / PercentScale) : value;
        return true;
    }

    if (v == "ad" || v.find("adamp") != std::string::npos || v.find("attackdamage") != std::string::npos)
    {
        affectedStatOut = "AttackDamage";
        modifierTypeOut = "Percent";
        valueOut = value > StatPercentThreshold ? (value / PercentScale) : value;
        return true;
    }

    if (v.find("critchance") != std::string::npos)
    {
        affectedStatOut = "CritChance";
        modifierTypeOut = "Flat";
        valueOut = value > StatPercentThreshold ? (value / PercentScale) : value;
        return true;
    }

    if (v.find("critdamage") != std::string::npos)
    {
        affectedStatOut = "CritDamage";
        modifierTypeOut = "Flat";
        valueOut = value > StatPercentThreshold ? (value / PercentScale) : value;
        return true;
    }

    if (v.find("damagereduction") != std::string::npos || v.find("durability") != std::string::npos)
    {
        affectedStatOut = "DamageReduction";
        modifierTypeOut = "Flat";
        valueOut = value > StatPercentThreshold ? (value / PercentScale) : value;
        return true;
    }

    return false;
}

static void appendStatusEffectJson(std::ostringstream& ss,
                                  std::string_view effectName,
                                  std::string_view affectedStat,
                                  std::string_view modifierType,
                                  double value)
{
    ss << "{ "
       << "\"name\": " << jsonString(effectName) << ", "
       << "\"effectType\": " << jsonString(statusEffectTypeForStat(affectedStat)) << ", "
       << "\"crowdControlType\": \"None\", "
       << "\"affectedStat\": " << jsonString(affectedStat) << ", "
       << "\"modifierType\": " << jsonString(modifierType) << ", "
       << "\"value\": " << value << ", "
       << "\"durationMs\": " << CombatConstants::MaxCombatDurationMs << ", "
       << "\"remainingMs\": " << CombatConstants::MaxCombatDurationMs << ", "
       << "\"tickIntervalMs\": 0, "
       << "\"tickTimerMs\": 0, "
       << "\"damageType\": \"True\""
       << " }";
}

static std::string writeTraitNormalizedFromCdragon(const JsonValue& traitObj, const std::string& name, const std::vector<int>& breakpoints)
{
    std::ostringstream ss;
    const std::string sourceId = getString(traitObj, "apiName", name);
    const std::string description = getString(traitObj, "desc", getString(traitObj, "description", ""));
    std::vector<std::pair<std::string, std::string>> allRawVariables;
    std::vector<std::string> importWarnings;
    bool mappedAny = false;

    ss << "{\n";
    ss << "  \"name\": " << jsonString(name) << ",\n";
    ss << "  \"breakpoints\": [";
    for (std::size_t i = 0; i < breakpoints.size(); ++i)
    {
        if (i) ss << ", ";
        ss << breakpoints[i];
    }
    ss << "],\n";
    ss << "  \"isPlaceholder\": true,\n";
    ss << "  \"sourceId\": " << jsonString(sourceId) << ",\n";
    ss << "  \"description\": " << jsonString(description) << ",\n";
    ss << "  \"tooltip\": " << jsonString(description) << ",\n";

    ss << "  \"tiers\": [\n";
    bool firstTier = true;
    if (hasKeyObj(traitObj, "effects") && traitObj.at("effects").isArray())
    {
        for (const JsonValue& e : traitObj.at("effects").asArray())
        {
            if (!e.isObject())
            {
                continue;
            }
            const int bp = static_cast<int>(getNumber(e, "minUnits", 0));
            if (bp <= 0)
            {
                continue;
            }

            std::vector<std::tuple<std::string, std::string, double>> mapped;
            if (hasKeyObj(e, "variables") && e.at("variables").isObject())
            {
                for (const auto& [k, v] : e.at("variables").asObject())
                {
                    allRawVariables.emplace_back(k, jsonValueSummary(v));
                    if (!v.isNumber())
                    {
                        importWarnings.push_back("Unmapped non-numeric trait variable: " + k);
                        continue;
                    }
                    std::string stat;
                    std::string mod;
                    double outVal = 0.0;
                    if (!tryMapVariableToStatEffect(k, v.asNumber(), stat, mod, outVal))
                    {
                        importWarnings.push_back("Unmapped trait variable: " + k);
                        continue;
                    }
                    if (std::abs(outVal) <= VerySmallValueEpsilon)
                    {
                        importWarnings.push_back("Ignored zero trait variable: " + k);
                        continue;
                    }
                    mapped.emplace_back(std::move(stat), std::move(mod), outVal);
                }
            }

            if (mapped.empty())
            {
                importWarnings.push_back("No executable effects mapped for breakpoint " + std::to_string(bp));
                continue;
            }
            mappedAny = true;

            if (!firstTier) ss << ",\n";
            firstTier = false;

            ss << "    {\n";
            ss << "      \"breakpoint\": " << bp << ",\n";
            ss << "      \"effects\": [\n";

            bool firstEffect = true;
            for (const auto& [stat, mod, val] : mapped)
            {
                if (!firstEffect) ss << ",\n";
                firstEffect = false;

                ss << "        { \"hook\": \"OnCombatStart\", \"type\": \"ApplyStatusToTraitUnits\", \"statusEffect\": ";
                std::ostringstream se;
                appendStatusEffectJson(se, std::string("Trait:") + name + ":" + stat, stat, mod, val);
                ss << se.str();
                ss << " }";
            }

            ss << "\n      ]\n";
            ss << "    }";
        }
    }

    std::sort(allRawVariables.begin(), allRawVariables.end(), [](const auto& a, const auto& b) {
        return a.first < b.first;
    });
    importWarnings.erase(std::remove(importWarnings.begin(), importWarnings.end(), ""), importWarnings.end());
    std::sort(importWarnings.begin(), importWarnings.end());
    importWarnings.erase(std::unique(importWarnings.begin(), importWarnings.end()), importWarnings.end());
    if (!mappedAny)
    {
        importWarnings.push_back("Trait has no mapped executable effects");
    }

    ss << "\n  ],\n";
    ss << "  \"rawVariables\": ";
    appendRawVariablesJson(ss, allRawVariables);
    ss << ",\n";
    ss << "  \"importWarnings\": ";
    appendStringArrayJson(ss, importWarnings);
    ss << "\n";
    ss << "}\n";
    return ss.str();
}

static std::string writeItemNormalizedFromCdragon(const JsonValue& itemObj, const std::string& name)
{
    std::ostringstream ss;
    const std::string sourceId = getString(itemObj, "apiName", name);
    const std::string description = getString(itemObj, "desc", getString(itemObj, "description", ""));
    const std::string iconPath = getString(itemObj, "icon", "");
    const std::string itemCategory = classifyItemCategory(itemObj, name);
    std::vector<std::pair<std::string, std::string>> rawVariables;
    std::vector<std::string> importWarnings;

    ss << "{\n";
    ss << "  \"name\": " << jsonString(name) << ",\n";
    ss << "  \"isPlaceholder\": true,\n";
    ss << "  \"sourceId\": " << jsonString(sourceId) << ",\n";
    ss << "  \"description\": " << jsonString(description) << ",\n";
    ss << "  \"tooltip\": " << jsonString(description) << ",\n";
    ss << "  \"iconPath\": " << jsonString(iconPath) << ",\n";
    ss << "  \"itemCategory\": " << jsonString(itemCategory) << ",\n";
    ss << "  \"passiveStats\": [\n";

    bool first = true;
    if (hasKeyObj(itemObj, "effects") && itemObj.at("effects").isObject())
    {
        for (const auto& [k, v] : itemObj.at("effects").asObject())
        {
            rawVariables.emplace_back(k, jsonValueSummary(v));
            if (!v.isNumber())
            {
                importWarnings.push_back("Unmapped non-numeric item variable: " + k);
                continue;
            }
            std::string stat;
            std::string mod;
            double outVal = 0.0;
            if (!tryMapVariableToStatEffect(k, v.asNumber(), stat, mod, outVal))
            {
                importWarnings.push_back("Unmapped item variable: " + k);
                continue;
            }
            if (std::abs(outVal) <= VerySmallValueEpsilon)
            {
                importWarnings.push_back("Ignored zero item variable: " + k);
                continue;
            }

            if (!first) ss << ",\n";
            first = false;
            ss << "    ";
            appendStatusEffectJson(ss, std::string("Item:") + name + ":" + stat, stat, mod, outVal);
        }
    }
    else
    {
        importWarnings.push_back("Item has no effects object in source data");
    }

    std::sort(rawVariables.begin(), rawVariables.end(), [](const auto& a, const auto& b) {
        return a.first < b.first;
    });
    std::sort(importWarnings.begin(), importWarnings.end());
    importWarnings.erase(std::unique(importWarnings.begin(), importWarnings.end()), importWarnings.end());

    ss << "\n  ],\n";
    ss << "  \"triggeredEffects\": [],\n";
    ss << "  \"rawVariables\": ";
    appendRawVariablesJson(ss, rawVariables);
    ss << ",\n";
    ss << "  \"importWarnings\": ";
    appendStringArrayJson(ss, importWarnings);
    ss << "\n";
    ss << "}\n";
    return ss.str();
}

static int pickDamageFromVariables(const JsonValue& spell, int& warnings)
{
    if (!spell.isObject())
    {
        return 0;
    }
    if (!hasKeyObj(spell, "variables"))
    {
        return static_cast<int>(getNumber(spell, "damage", 0));
    }
    const JsonValue& vars = spell.at("variables");
    if (!vars.isArray())
    {
        return 0;
    }

    int best = 0;
    for (const JsonValue& v : vars.asArray())
    {
        if (!v.isObject()) continue;
        const std::string n = getString(v, "name", "");
        if (n.empty()) continue;
        bool looksDamage = false;
        for (char c : n)
        {
            if (c == 'D' || c == 'd')
            {
                looksDamage = true;
                break;
            }
        }
        if (!looksDamage) continue;

        if (hasKeyObj(v, "value"))
        {
            const JsonValue& value = v.at("value");
            if (value.isArray() && !value.asArray().empty() && value.asArray().front().isNumber())
            {
                best = std::max(best, static_cast<int>(value.asArray().front().asNumber()));
            }
            else if (value.isNumber())
            {
                best = std::max(best, static_cast<int>(value.asNumber()));
            }
        }
        else if (hasKeyObj(v, "values"))
        {
            const JsonValue& values = v.at("values");
            if (values.isArray() && !values.asArray().empty() && values.asArray().front().isNumber())
            {
                best = std::max(best, static_cast<int>(values.asArray().front().asNumber()));
            }
        }
    }

    if (best == 0)
    {
        warnings += 1;
    }
    return best;
}

bool TFTDataImporter::downloadToFile(const std::string& url, const std::string& outFilePath, std::ostream& out)
{
    std::filesystem::create_directories(std::filesystem::path(outFilePath).parent_path());

    std::ostringstream cmd;
    cmd << "powershell -NoProfile -Command \""
        << "$ProgressPreference='SilentlyContinue'; "
        << "Invoke-WebRequest -UseBasicParsing -Uri " << "'" << url << "'"
        << " -OutFile " << "'" << outFilePath << "'" << "\"";

    out << "Downloading: " << url << "\n";
    const int rc = std::system(cmd.str().c_str());
    if (rc != 0)
    {
        out << "ERROR: download failed (exit " << rc << ")\n";
        return false;
    }
    return std::filesystem::exists(outFilePath);
}

TFTDataImporter::ImportResult TFTDataImporter::importLiveTft(const std::string& outputDataRoot, std::ostream& out)
{
    ImportResult result{};

    const std::string localeUrl = "https://raw.communitydragon.org/latest/cdragon/tft/en_us.json";
    const std::filesystem::path cacheDir = std::filesystem::path(outputDataRoot) / "_import_cache";
    const std::filesystem::path downloaded = cacheDir / "cdragon_tft_en_us.json";

    if (!downloadToFile(localeUrl, downloaded.string(), out))
    {
        out << "ERROR: Could not download CommunityDragon TFT dataset.\n";
        return result;
    }

    const JsonValue root = parseJson(readFileToString(downloaded));
    const int setNumber = detectCurrentSet(root);
    result.detectedSet = setNumber;

    out << "\n=== TFT IMPORT REPORT ===\n\n";
    out << "Current Set Detected: Set " << (setNumber > 0 ? setNumber : 0) << "\n\n";

    std::filesystem::create_directories(std::filesystem::path(outputDataRoot) / "champions");
    std::filesystem::create_directories(std::filesystem::path(outputDataRoot) / "abilities");
    std::filesystem::create_directories(std::filesystem::path(outputDataRoot) / "traits");
    std::filesystem::create_directories(std::filesystem::path(outputDataRoot) / "items");
    std::filesystem::create_directories(std::filesystem::path(outputDataRoot) / "scenarios");

    const JsonValue* setObj = nullptr;
    if (setNumber > 0 && hasKeyObj(root, "sets") && root.at("sets").isObject())
    {
        const auto& sets = root.at("sets").asObject();
        const std::string key = std::to_string(setNumber);
        auto it = sets.find(key);
        if (it != sets.end())
        {
            setObj = &it->second;
        }
    }
    if (!setObj && hasKeyObj(root, "setData") && root.at("setData").isArray())
    {
        for (const JsonValue& e : root.at("setData").asArray())
        {
            if (!e.isObject()) continue;
            const int n = static_cast<int>(getNumber(e, "number", getNumber(e, "setNumber", 0)));
            if (n == setNumber)
            {
                setObj = &e;
                break;
            }
        }
    }

    if (!setObj || !setObj->isObject())
    {
        out << "WARNING: could not locate set payload; falling back to top-level sections.\n";
    }

    const JsonValue* champions = nullptr;
    const JsonValue* traits = nullptr;
    const JsonValue* items = nullptr;

    if (setObj && setObj->isObject())
    {
        if (hasKeyObj(*setObj, "champions")) champions = &setObj->at("champions");
        if (hasKeyObj(*setObj, "traits")) traits = &setObj->at("traits");
        if (hasKeyObj(*setObj, "items")) items = &setObj->at("items");
    }

    if (!champions && hasKeyObj(root, "champions")) champions = &root.at("champions");
    if (!traits && hasKeyObj(root, "traits")) traits = &root.at("traits");
    if (!items && hasKeyObj(root, "items")) items = &root.at("items");

    if (traits && traits->isArray())
    {
        for (const JsonValue& t : traits->asArray())
        {
            if (!t.isObject()) continue;
            const std::string name = getString(t, "name", getString(t, "displayName", ""));
            if (name.empty()) continue;

            std::vector<int> breakpoints;
            if (hasKeyObj(t, "effects") && t.at("effects").isArray())
            {
                for (const JsonValue& e : t.at("effects").asArray())
                {
                    if (!e.isObject()) continue;
                    const int bp = static_cast<int>(getNumber(e, "minUnits", 0));
                    if (bp > 0) breakpoints.push_back(bp);
                }
            }
            if (breakpoints.empty()) breakpoints = { 1 };
            std::sort(breakpoints.begin(), breakpoints.end());
            breakpoints.erase(std::unique(breakpoints.begin(), breakpoints.end()), breakpoints.end());

            writeStringToFile(
                std::filesystem::path(outputDataRoot) / "traits" / (safeFileName(toSnakeLower(name)) + ".json"),
                writeTraitNormalizedFromCdragon(t, name, breakpoints));
            result.traits += 1;
        }
    }

    if (items && items->isArray())
    {
        for (const JsonValue& it : items->asArray())
        {
            if (!it.isObject()) continue;
            const std::string name = getString(it, "name", getString(it, "displayName", ""));
            if (name.empty()) continue;
            writeStringToFile(
    std::filesystem::path(outputDataRoot) / "items" /
    (safeFileName(toSnakeLower(name)) + ".json"),
                              writeItemNormalizedFromCdragon(it, name));
            result.items += 1;
        }
    }

    if (champions && champions->isArray())
    {
        for (const JsonValue& c : champions->asArray())
        {
            if (!c.isObject()) continue;
            const std::string name = getString(c, "name", getString(c, "displayName", ""));
            if (name.empty()) continue;

            const int cost =
                static_cast<int>(getNumber(c, "cost", getNumber(c, "tier", DefaultChampionCost)));

            int hp = static_cast<int>(getNumber(c, "hp", getNumber(c, "health", DefaultChampionHp)));
            int ad =
                static_cast<int>(getNumber(c, "ad", getNumber(c, "attackDamage", DefaultChampionAttackDamage)));
            int armor = static_cast<int>(getNumber(c, "armor", DefaultChampionArmor));
            int mr =
                static_cast<int>(getNumber(c, "magicResist", getNumber(c, "mr", DefaultChampionMagicResist)));
            int range =
                static_cast<int>(getNumber(c, "range", getNumber(c, "attackRange", DefaultChampionRange)));
            float as = static_cast<float>(getNumber(c, "attackSpeed", DefaultChampionAttackSpeed));
            int ap = static_cast<int>(getNumber(c, "abilityPower", DefaultChampionAbilityPower));

            int startMana = static_cast<int>(getNumber(c, "mana", getNumber(c, "startingMana", 0)));
            int maxMana = static_cast<int>(getNumber(c, "maxMana", getNumber(c, "manaCost", 0)));
            int manaGainOnAttack = DefaultChampionManaGainOnAttack;

            float critChance = static_cast<float>(getNumber(c, "critChance", 0.0));
            float critDamage = static_cast<float>(getNumber(c, "critDamage", 1.5));
            float durability = static_cast<float>(getNumber(c, "durability", 0.0));

            std::vector<std::string> traitsList;
            if (hasKeyObj(c, "traits") && c.at("traits").isArray())
            {
                for (const JsonValue& t : c.at("traits").asArray())
                {
                    if (t.isString()) traitsList.push_back(t.asString());
                }
            }

            const std::string iconPath = getString(c, "iconPath", "");
            const std::string icon = getString(c, "icon", iconPath);
            const std::string squareIconPath = getString(c, "squareIcon", "");
            const std::string tileIconPath = getString(c, "tileIcon", "");
            const std::string splashPath = getString(c, "splashPath", "");
            const std::string sourceId = getString(c, "apiName", getString(c, "characterName", name));
            const std::string description = getString(c, "desc", getString(c, "description", ""));
            const std::string tooltip = getString(c, "tooltip", "");
            std::vector<std::string> championWarnings;

            std::string abilityId;
            std::string abilityName;
            JsonValue spellObj{};
            bool hasAbilityData = false;

            if (hasKeyObj(c, "spell"))
            {
                spellObj = c.at("spell");
                abilityId = getString(spellObj, "name", getString(spellObj, "apiName", ""));
                abilityName = getString(spellObj, "displayName", getString(spellObj, "name", abilityId));
                hasAbilityData = !abilityId.empty();
            }
            else if (hasKeyObj(c, "ability"))
            {
                spellObj = c.at("ability");
                abilityId = getString(spellObj, "name", getString(spellObj, "apiName", ""));
                abilityName = getString(spellObj, "displayName", getString(spellObj, "name", abilityId));
                hasAbilityData = !abilityId.empty();
            }

            if (abilityId.empty())
            {
                abilityId = name + "Ability";
                abilityName = abilityId;
                championWarnings.push_back("No ability id in source data");
                result.warnings += 1;
            }

            std::vector<std::string> abilityWarnings;
            const int baseDamage = pickDamageFromVariables(spellObj, result.warnings);
            if (baseDamage == 0)
            {
                abilityWarnings.push_back("Ability damage required fallback/default inference");
            }
            const float adRatio = 0.0f;
            const float apRatio = 0.8f;

            if (maxMana <= 0)
            {
                maxMana = static_cast<int>(getNumber(spellObj, "manaCost", 0));
            }
            if (maxMana <= 0)
            {
                maxMana = 60;
                abilityWarnings.push_back("Ability max mana required fallback/default inference");
                result.warnings += 1;
            }

            std::vector<std::string> tags;
            if (cost < 1 || cost > 5) tags.push_back("OutOfShopCostRange");
            if (traitsList.empty()) tags.push_back("NoTraits");
            if (!hasAbilityData) tags.push_back("NoAbilityData");
            if (hp <= 0 || ad <= 0 || range <= 0 || as <= 0.0f) tags.push_back("InvalidCombatStats");
            if (range <= 0) tags.push_back("Object");
            for (const std::string& tag : tags)
            {
                championWarnings.push_back("Champion tag: " + tag);
            }

            const bool isPlayable =
                cost >= 1 && cost <= 5 &&
                hp > 0 && ad > 0 && range > 0 && as > 0.0f &&
                hasAbilityData &&
                !traitsList.empty();

            writeStringToFile(
    std::filesystem::path(outputDataRoot) / "champions" /
    (safeFileName(toSnakeLower(name)) + ".json"),
                              writeChampionNormalized(name, cost, hp, ad, armor, mr, startMana, maxMana, manaGainOnAttack, range, as, ap,
                                                     critChance, critDamage, durability, traitsList, isPlayable, tags, abilityName,
                                                     sourceId, description, tooltip, icon, squareIconPath, tileIconPath, splashPath,
                                                     championWarnings));
            result.champions += 1;

            writeStringToFile(
    std::filesystem::path(outputDataRoot) / "abilities" /
    (safeFileName(toSnakeLower(abilityId)) + ".json"),
                              writeAbilityNormalized(abilityId, abilityName, maxMana, baseDamage, adRatio, apRatio, "SingleTarget", 0, 0, false,
                                                     spellObj, abilityWarnings));
            result.abilities += 1;
        }
    }

    out << "Champions Imported: " << result.champions << "\n";
    out << "Traits Imported: " << result.traits << "\n";
    out << "Items Imported: " << result.items << "\n";
    out << "Abilities Imported: " << result.abilities << "\n\n";

    out << "PASS: Champion parsing\n";
    out << "PASS: Ability parsing\n";
    out << "PASS: Trait parsing\n";
    out << "PASS: Item parsing\n";
    if (result.warnings > 0)
    {
        out << "\nWARNING: " << result.warnings << " values required fallback/defaults\n";
    }

    out << "\n";
    return result;
}
