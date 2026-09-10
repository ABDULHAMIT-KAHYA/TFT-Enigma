#include "ai/LearnedPolicyModel.hpp"

#include "content/ContentManager.hpp"
#include "core/Json.hpp"
#include "macro/PlayerState.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace
{
std::string readFile(const std::string& path)
{
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file)
    {
        throw std::runtime_error("unable to open model file: " + path);
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

std::string optionalString(const JsonValue& obj, std::string_view key, std::string fallback)
{
    if (!obj.isObject() || !obj.hasKey(key) || !obj.at(key).isString())
    {
        return fallback;
    }
    return obj.at(key).asString();
}

float optionalFloat(const JsonValue& obj, std::string_view key, float fallback)
{
    if (!obj.isObject() || !obj.hasKey(key) || !obj.at(key).isNumber())
    {
        return fallback;
    }
    return static_cast<float>(obj.at(key).asNumber());
}
}

bool LearnedPolicyModel::load(const std::string& path, std::string& error)
{
    try
    {
        const JsonValue root = parseJson(readFile(path));
        if (!root.isObject())
        {
            error = "model root is not an object";
            return false;
        }
        if (optionalString(root, "format", "") != "tft-galaxy-linear-model-v1")
        {
            error = "unsupported model format";
            return false;
        }
        if (optionalString(root, "kind", "") != "policy")
        {
            error = "model kind is not policy";
            return false;
        }
        if (!root.hasKey("weights") || !root.at("weights").isArray())
        {
            error = "model weights missing";
            return false;
        }
        std::vector<float> weights;
        for (const JsonValue& value : root.at("weights").asArray())
        {
            if (!value.isNumber())
            {
                error = "model weight is not numeric";
                return false;
            }
            weights.push_back(static_cast<float>(value.asNumber()));
        }
        if (weights.empty())
        {
            error = "model has no weights";
            return false;
        }
        weights_ = std::move(weights);
        bias_ = optionalFloat(root, "bias", 0.0f);
        path_ = path;
        error.clear();
        return true;
    }
    catch (const std::exception& ex)
    {
        error = ex.what();
        return false;
    }
}

bool LearnedPolicyModel::isLoaded() const
{
    return !weights_.empty();
}

const std::string& LearnedPolicyModel::path() const
{
    return path_;
}

float LearnedPolicyModel::score(const StateFeatures& state, const ActionEncoding& action) const
{
    if (weights_.empty())
    {
        return 0.0f;
    }
    const std::vector<float> features = TrainingData::encodeStateActionFeatures(state, action);
    const std::size_t n = std::min(features.size(), weights_.size());
    float total = bias_;
    for (std::size_t i = 0; i < n; ++i)
    {
        total += features[i] * weights_[i];
    }
    return total;
}

MacroAction LearnedPolicyModel::chooseAction(const PlayerState& player,
                                             const ContentManager& content,
                                             const std::vector<MacroAction>& legalActions,
                                             const EnemySnapshot* enemy,
                                             int stage,
                                             int roundIndex) const
{
    if (legalActions.empty())
    {
        return MacroAction{ MacroActionType::EndTurn };
    }
    const StateFeatures state = TrainingData::encodeState(player, content, roundIndex, stage, 0, enemy);
    std::size_t bestIndex = 0;
    float bestScore = score(state, TrainingData::encodeAction(legalActions[0], player, content));
    for (std::size_t i = 1; i < legalActions.size(); ++i)
    {
        const float s = score(state, TrainingData::encodeAction(legalActions[i], player, content));
        if (s > bestScore)
        {
            bestScore = s;
            bestIndex = i;
        }
    }
    return legalActions[bestIndex];
}
