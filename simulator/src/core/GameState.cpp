#include "core/GameState.hpp"
#include "combat/CombatEventExecutor.hpp"
#include "content/ContentManager.hpp"
#include "constants/GameConstants.hpp"
#include "core/RandomManager.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <utility>

namespace
{
    void hashMix(std::uint64_t& h, std::uint64_t v)
    {
        h ^= v + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
    }

    void hashString(std::uint64_t& h, const std::string& s)
    {
        for (unsigned char c : s)
        {
            hashMix(h, static_cast<std::uint64_t>(c));
        }
        hashMix(h, static_cast<std::uint64_t>(s.size()));
    }

    std::int64_t hashFloat(float v)
    {
        return static_cast<std::int64_t>(std::llround(static_cast<double>(v) * 1000000.0));
    }

    std::string jsonEscape(const std::string& s)
    {
        std::ostringstream out;
        for (char ch : s)
        {
            switch (ch)
            {
                case '\\': out << "\\\\"; break;
                case '"': out << "\\\""; break;
                case '\n': out << "\\n"; break;
                case '\r': out << "\\r"; break;
                case '\t': out << "\\t"; break;
                default: out << ch; break;
            }
        }
        return out.str();
    }

    int asInt(TeamId team)
    {
        return team == TeamId::TeamA ? 0 : 1;
    }
}

GameState::GameState(Board board, std::vector<Unit> units, Logger logger, const ContentManager& content)
    : board_(std::move(board)),
      units_(std::move(units)),
      logger_(std::move(logger)),
      content_(&content),
      time_ms_(0),
      dt_ms_(GameConstants::DefaultDtMs),
      scheduled_events_{},
      scheduled_event_count_(0),
      executed_event_count_(0),
      max_scheduled_queue_size_(0),
      tick_count_(0),
      next_event_sequence_(1),
      next_unit_id_{1},
      combat_timed_out_(false),
      snapshot_recording_(false)
{
    assignMissingUnitIds();
}

void GameState::assignMissingUnitIds()
{
    std::uint64_t maxSeen = 0;
    for (Unit& unit : units_)
    {
        if (isValid(unit.id()))
        {
            maxSeen = std::max(maxSeen, unit.id().value);
            continue;
        }
        unit.assignId(next_unit_id_);
        maxSeen = std::max(maxSeen, next_unit_id_.value);
        next_unit_id_.value += 1;
    }
    next_unit_id_.value = std::max(next_unit_id_.value, maxSeen + 1);
}

Board& GameState::board() { return board_; }
const Board& GameState::board() const { return board_; }
std::vector<Unit>& GameState::units() { return units_; }
const std::vector<Unit>& GameState::units() const { return units_; }

Unit* GameState::findUnit(UnitId id)
{
    if (!isValid(id)) return nullptr;
    for (Unit& unit : units_)
    {
        if (unit.id() == id) return &unit;
    }
    return nullptr;
}

const Unit* GameState::findUnit(UnitId id) const
{
    if (!isValid(id)) return nullptr;
    for (const Unit& unit : units_)
    {
        if (unit.id() == id) return &unit;
    }
    return nullptr;
}

std::optional<std::size_t> GameState::findUnitIndex(UnitId id) const
{
    if (!isValid(id)) return std::nullopt;
    for (std::size_t i = 0; i < units_.size(); ++i)
    {
        if (units_[i].id() == id) return i;
    }
    return std::nullopt;
}

UnitId GameState::allocateUnitId()
{
    UnitId out = next_unit_id_;
    next_unit_id_.value += 1;
    return out;
}

Logger& GameState::logger() { return logger_; }
const Logger& GameState::logger() const { return logger_; }
const ContentManager& GameState::content() const { return *content_; }
std::int32_t GameState::timeMs() const { return time_ms_; }
std::int32_t GameState::dtMs() const { return dt_ms_; }

void GameState::setDtMs(std::int32_t dtMs)
{
    dt_ms_ = dtMs;
}

void GameState::advanceTick()
{
    time_ms_ += dt_ms_;
    tick_count_ += 1;
}

void GameState::scheduleCombatEvent(CombatEvent event)
{
    if (event.sequence == 0)
    {
        event.sequence = next_event_sequence_++;
    }
    if (event.debugName.empty())
    {
        event.debugName = toString(event.type);
    }
    scheduled_events_.push_back(std::move(event));
    scheduled_event_count_ += 1;
    max_scheduled_queue_size_ = std::max(max_scheduled_queue_size_, scheduled_events_.size());

    std::ostringstream ss;
    const CombatEvent& e = scheduled_events_.back();
    ss << time_ms_ << "ms Scheduled " << e.debugName
       << "#" << e.sequence
       << " type=" << toString(e.type)
       << " source=" << e.sourceId
       << " target=" << e.targetId
       << " at " << e.executeAtMs << "ms";
    logger_.combat(ss.str());
}

void GameState::processCombatEvents()
{
    if (scheduled_events_.empty()) return;

    std::stable_sort(
        scheduled_events_.begin(),
        scheduled_events_.end(),
        [](const CombatEvent& a, const CombatEvent& b)
        {
            if (a.executeAtMs != b.executeAtMs) return a.executeAtMs < b.executeAtMs;
            return a.sequence < b.sequence;
        }
    );

    std::vector<CombatEvent> due;
    std::size_t i = 0;
    for (; i < scheduled_events_.size(); ++i)
    {
        if (scheduled_events_[i].executeAtMs > time_ms_) break;
        due.push_back(scheduled_events_[i]);
    }
    if (i > 0)
    {
        scheduled_events_.erase(scheduled_events_.begin(), scheduled_events_.begin() + static_cast<std::ptrdiff_t>(i));
    }

    for (const CombatEvent& event : due)
    {
        std::ostringstream ss;
        ss << time_ms_ << "ms Executing " << event.debugName
           << "#" << event.sequence
           << " type=" << toString(event.type)
           << " source=" << event.sourceId
           << " target=" << event.targetId;
        logger_.combat(ss.str());
        executed_event_count_ += 1;
        CombatEventExecutor::execute(*this, event);
    }
}

const std::vector<CombatEvent>& GameState::scheduledEvents() const { return scheduled_events_; }
std::int64_t GameState::scheduledEventCount() const { return scheduled_event_count_; }
std::int64_t GameState::executedEventCount() const { return executed_event_count_; }
std::size_t GameState::maxScheduledQueueSize() const { return max_scheduled_queue_size_; }
std::int64_t GameState::tickCount() const { return tick_count_; }

bool GameState::hasAlive(TeamId team) const
{
    for (const Unit& unit : units_)
    {
        if (unit.getTeamId() == team && unit.isAlive()) return true;
    }
    return false;
}

void GameState::markCombatTimedOut() { combat_timed_out_ = true; }
bool GameState::combatTimedOut() const { return combat_timed_out_; }

TraitRuntimeState& GameState::traitRuntime() { return trait_runtime_; }
const TraitRuntimeState& GameState::traitRuntime() const { return trait_runtime_; }

bool GameState::canTriggerItemEffect(std::size_t ownerIndex,
                                     std::size_t itemIndex,
                                     std::size_t effectIndex,
                                     std::int32_t cooldownMs,
                                     bool oncePerCombat) const
{
    if (cooldownMs <= 0 && !oncePerCombat) return true;
    for (const ItemEffectGateState& state : item_effect_gate_state_)
    {
        if (state.ownerIndex != ownerIndex || state.itemIndex != itemIndex || state.effectIndex != effectIndex) continue;
        if (oncePerCombat && state.triggered) return false;
        if (cooldownMs > 0 && state.triggered && time_ms_ - state.lastTriggeredMs < cooldownMs) return false;
        return true;
    }
    return true;
}

void GameState::recordItemEffectTrigger(std::size_t ownerIndex,
                                        std::size_t itemIndex,
                                        std::size_t effectIndex)
{
    for (ItemEffectGateState& state : item_effect_gate_state_)
    {
        if (state.ownerIndex == ownerIndex && state.itemIndex == itemIndex && state.effectIndex == effectIndex)
        {
            state.lastTriggeredMs = time_ms_;
            state.triggered = true;
            return;
        }
    }
    ItemEffectGateState state{};
    state.ownerIndex = ownerIndex;
    state.itemIndex = itemIndex;
    state.effectIndex = effectIndex;
    state.lastTriggeredMs = time_ms_;
    state.triggered = true;
    item_effect_gate_state_.push_back(state);
}

UnitId GameState::nextUnitId() const
{
    return next_unit_id_;
}

GameState GameState::clone() const
{
    GameState copy(board_, units_, logger_, *content_);
    copy.time_ms_ = time_ms_;
    copy.dt_ms_ = dt_ms_;
    copy.scheduled_events_ = scheduled_events_;
    copy.scheduled_event_count_ = scheduled_event_count_;
    copy.executed_event_count_ = executed_event_count_;
    copy.max_scheduled_queue_size_ = max_scheduled_queue_size_;
    copy.tick_count_ = tick_count_;
    copy.next_event_sequence_ = next_event_sequence_;
    copy.next_unit_id_ = next_unit_id_;
    copy.trait_runtime_ = trait_runtime_;
    copy.combat_timed_out_ = combat_timed_out_;
    copy.item_effect_gate_state_ = item_effect_gate_state_;
    copy.snapshot_recording_ = snapshot_recording_;
    copy.snapshots_ = snapshots_;
    return copy;
}

std::uint64_t GameState::deterministicHash() const
{
    std::uint64_t h = 0xcbf29ce484222325ull;
    hashMix(h, static_cast<std::uint64_t>(time_ms_));
    hashMix(h, static_cast<std::uint64_t>(dt_ms_));
    hashMix(h, static_cast<std::uint64_t>(tick_count_));
    hashMix(h, next_unit_id_.value);
    hashMix(h, next_event_sequence_);
    hashMix(h, RandomManager::global().seed());
    hashMix(h, RandomManager::global().state());

    std::vector<const Unit*> ordered;
    ordered.reserve(units_.size());
    for (const Unit& unit : units_) ordered.push_back(&unit);
    std::sort(ordered.begin(), ordered.end(), [](const Unit* a, const Unit* b) { return a->id() < b->id(); });
    for (const Unit* unit : ordered)
    {
        hashMix(h, unit->id().value);
        hashString(h, unit->getName());
        hashMix(h, static_cast<std::uint64_t>(asInt(unit->getTeamId())));
        hashMix(h, static_cast<std::uint64_t>(unit->getPosition().x));
        hashMix(h, static_cast<std::uint64_t>(unit->getPosition().y));
        hashMix(h, static_cast<std::uint64_t>(unit->getHp()));
        hashMix(h, static_cast<std::uint64_t>(unit->getMaxHp()));
        hashMix(h, static_cast<std::uint64_t>(unit->getMana()));
        hashMix(h, static_cast<std::uint64_t>(unit->getMaxMana()));
        hashMix(h, static_cast<std::uint64_t>(unit->isAlive() ? 1 : 0));
        hashMix(h, static_cast<std::uint64_t>(unit->isCasting() ? 1 : 0));
        hashMix(h, static_cast<std::uint64_t>(unit->isManaLocked() ? 1 : 0));
        hashMix(h, static_cast<std::uint64_t>(unit->getAttackTimerMs()));
        hashMix(h, static_cast<std::uint64_t>(hashFloat(unit->getAttackSpeed())));
        hashMix(h, static_cast<std::uint64_t>(unit->getAttackRange()));
        hashMix(h, static_cast<std::uint64_t>(unit->items().size()));
        for (const Item& item : unit->items()) hashString(h, item.name);
        hashMix(h, static_cast<std::uint64_t>(unit->statusEffects().size()));
        for (const StatusEffect& e : unit->statusEffects())
        {
            hashString(h, e.name);
            hashMix(h, static_cast<std::uint64_t>(static_cast<int>(e.effectType)));
            hashMix(h, static_cast<std::uint64_t>(static_cast<int>(e.crowdControlType)));
            hashMix(h, static_cast<std::uint64_t>(static_cast<int>(e.affectedStat)));
            hashMix(h, static_cast<std::uint64_t>(static_cast<int>(e.modifierType)));
            hashMix(h, static_cast<std::uint64_t>(hashFloat(e.value)));
            hashMix(h, static_cast<std::uint64_t>(e.durationMs));
            hashMix(h, static_cast<std::uint64_t>(e.remainingMs));
            hashMix(h, static_cast<std::uint64_t>(e.tickIntervalMs));
            hashMix(h, static_cast<std::uint64_t>(e.tickTimerMs));
            hashMix(h, static_cast<std::uint64_t>(static_cast<int>(e.damageType)));
        }
    }

    auto hashTraits = [&](const std::vector<ActiveTrait>& traits)
    {
        std::vector<ActiveTrait> sorted = traits;
        std::sort(sorted.begin(), sorted.end(), [](const ActiveTrait& a, const ActiveTrait& b) {
            if (a.traitName != b.traitName) return a.traitName < b.traitName;
            if (a.breakpoint != b.breakpoint) return a.breakpoint < b.breakpoint;
            return a.activeCount < b.activeCount;
        });
        for (const ActiveTrait& t : sorted)
        {
            hashString(h, t.traitName);
            hashMix(h, static_cast<std::uint64_t>(t.activeCount));
            hashMix(h, static_cast<std::uint64_t>(t.breakpoint));
        }
    };
    hashTraits(trait_runtime_.activeTraitsA);
    hashTraits(trait_runtime_.activeTraitsB);
    hashMix(h, static_cast<std::uint64_t>(trait_runtime_.nextPeriodicMsA));
    hashMix(h, static_cast<std::uint64_t>(trait_runtime_.nextPeriodicMsB));
    hashMix(h, static_cast<std::uint64_t>(trait_runtime_.nextAuraUpdateMsA));
    hashMix(h, static_cast<std::uint64_t>(trait_runtime_.nextAuraUpdateMsB));

    std::vector<CombatEvent> events = scheduled_events_;
    std::sort(events.begin(), events.end(), [](const CombatEvent& a, const CombatEvent& b) {
        if (a.executeAtMs != b.executeAtMs) return a.executeAtMs < b.executeAtMs;
        return a.sequence < b.sequence;
    });
    for (const CombatEvent& e : events)
    {
        hashMix(h, static_cast<std::uint64_t>(static_cast<int>(e.type)));
        hashMix(h, static_cast<std::uint64_t>(e.executeAtMs));
        hashMix(h, e.sequence);
        hashMix(h, e.sourceId.value);
        hashMix(h, e.targetId.value);
        hashMix(h, static_cast<std::uint64_t>(static_cast<int>(e.trigger)));
        hashMix(h, static_cast<std::uint64_t>(static_cast<int>(e.targetType)));
        hashMix(h, static_cast<std::uint64_t>(static_cast<int>(e.policy)));
        hashString(h, e.debugName);
        hashMix(h, static_cast<std::uint64_t>(e.projectile.rawDamage));
        hashMix(h, static_cast<std::uint64_t>(static_cast<int>(e.projectile.damageType)));
        hashMix(h, static_cast<std::uint64_t>(e.projectile.didCrit ? 1 : 0));
        hashMix(h, static_cast<std::uint64_t>(e.abilityEffect.damageFormula.baseDamage));
        hashMix(h, static_cast<std::uint64_t>(hashFloat(e.abilityEffect.damageFormula.adRatio)));
        hashMix(h, static_cast<std::uint64_t>(hashFloat(e.abilityEffect.damageFormula.apRatio)));
    }
    return h;
}

std::string GameState::snapshotJson() const
{
    std::ostringstream out;
    out << "{\n";
    out << "  \"timeMs\": " << time_ms_ << ",\n";
    out << "  \"dtMs\": " << dt_ms_ << ",\n";
    out << "  \"tick\": " << tick_count_ << ",\n";
    out << "  \"hash\": " << deterministicHash() << ",\n";
    out << "  \"rngSeed\": " << RandomManager::global().seed() << ",\n";
    out << "  \"rngState\": " << RandomManager::global().state() << ",\n";
    out << "  \"nextUnitId\": " << next_unit_id_.value << ",\n";
    out << "  \"units\": [\n";
    std::vector<const Unit*> ordered;
    for (const Unit& unit : units_) ordered.push_back(&unit);
    std::sort(ordered.begin(), ordered.end(), [](const Unit* a, const Unit* b) { return a->id() < b->id(); });
    for (std::size_t i = 0; i < ordered.size(); ++i)
    {
        const Unit& u = *ordered[i];
        out << "    {\"id\": " << u.id().value
            << ", \"name\": \"" << jsonEscape(u.getName()) << "\""
            << ", \"team\": \"" << (u.getTeamId() == TeamId::TeamA ? "A" : "B") << "\""
            << ", \"x\": " << u.getPosition().x
            << ", \"y\": " << u.getPosition().y
            << ", \"hp\": " << u.getHp()
            << ", \"maxHp\": " << u.getMaxHp()
            << ", \"mana\": " << u.getMana()
            << ", \"maxMana\": " << u.getMaxMana()
            << ", \"alive\": " << (u.isAlive() ? "true" : "false")
            << ", \"casting\": " << (u.isCasting() ? "true" : "false")
            << ", \"statuses\": [";
        for (std::size_t s = 0; s < u.statusEffects().size(); ++s)
        {
            if (s) out << ", ";
            out << "\"" << jsonEscape(u.statusEffects()[s].name) << "\"";
        }
        out << "]}" << (i + 1 == ordered.size() ? "" : ",") << "\n";
    }
    out << "  ],\n";
    out << "  \"events\": [\n";
    std::vector<CombatEvent> events = scheduled_events_;
    std::sort(events.begin(), events.end(), [](const CombatEvent& a, const CombatEvent& b) {
        if (a.executeAtMs != b.executeAtMs) return a.executeAtMs < b.executeAtMs;
        return a.sequence < b.sequence;
    });
    for (std::size_t i = 0; i < events.size(); ++i)
    {
        const CombatEvent& e = events[i];
        out << "    {\"time\": " << e.executeAtMs
            << ", \"sequence\": " << e.sequence
            << ", \"type\": \"" << toString(e.type) << "\""
            << ", \"source\": " << e.sourceId.value
            << ", \"target\": " << e.targetId.value
            << ", \"name\": \"" << jsonEscape(e.debugName) << "\"}"
            << (i + 1 == events.size() ? "" : ",") << "\n";
    }
    out << "  ]\n";
    out << "}\n";
    return out.str();
}

void GameState::setSnapshotRecording(bool enabled)
{
    snapshot_recording_ = enabled;
}

bool GameState::snapshotRecording() const
{
    return snapshot_recording_;
}

void GameState::captureSnapshot(std::string label)
{
    if (!snapshot_recording_) return;
    std::ostringstream out;
    out << "{\"label\":\"" << jsonEscape(label) << "\",\"state\":" << snapshotJson() << "}";
    snapshots_.push_back(out.str());
}

const std::vector<std::string>& GameState::snapshots() const
{
    return snapshots_;
}

