#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include "combat/CombatEvent.hpp"
#include "combat/TraitRuntime.hpp"
#include "core/Board.hpp"
#include "core/Logger.hpp"
#include "core/TeamId.hpp"
#include "core/Unit.hpp"
class ContentManager;

class GameState
{
public:
    GameState(Board board, std::vector<Unit> units, Logger logger, const ContentManager& content);

    Board& board();
    const Board& board() const;

    std::vector<Unit>& units();
    const std::vector<Unit>& units() const;

    Unit* findUnit(UnitId id);
    const Unit* findUnit(UnitId id) const;
    std::optional<std::size_t> findUnitIndex(UnitId id) const;
    UnitId allocateUnitId();

    Logger& logger();
    const Logger& logger() const;

    const ContentManager& content() const;

    std::int32_t timeMs() const;
    std::int32_t dtMs() const;
    void setDtMs(std::int32_t dtMs);
    void advanceTick();

    void scheduleCombatEvent(CombatEvent event);
    void processCombatEvents();
    const std::vector<CombatEvent>& scheduledEvents() const;

    std::int64_t scheduledEventCount() const;
    std::int64_t executedEventCount() const;
    std::size_t maxScheduledQueueSize() const;
    std::int64_t tickCount() const;

    bool hasAlive(TeamId team) const;

    TraitRuntimeState& traitRuntime();
    const TraitRuntimeState& traitRuntime() const;

    bool canTriggerItemEffect(std::size_t ownerIndex,
                              std::size_t itemIndex,
                              std::size_t effectIndex,
                              std::int32_t cooldownMs,
                              bool oncePerCombat) const;
    void recordItemEffectTrigger(std::size_t ownerIndex,
                                 std::size_t itemIndex,
                                 std::size_t effectIndex);

    UnitId nextUnitId() const;
    GameState clone() const;
    std::uint64_t deterministicHash() const;
    std::string snapshotJson() const;

    void setSnapshotRecording(bool enabled);
    bool snapshotRecording() const;
    void captureSnapshot(std::string label);
    const std::vector<std::string>& snapshots() const;

private:
    struct ItemEffectGateState
    {
        std::size_t ownerIndex = 0;
        std::size_t itemIndex = 0;
        std::size_t effectIndex = 0;
        std::int32_t lastTriggeredMs = 0;
        bool triggered = false;
    };

    void assignMissingUnitIds();

    Board board_;
    std::vector<Unit> units_;
    Logger logger_;
    const ContentManager* content_;
    std::int32_t time_ms_;
    std::int32_t dt_ms_;
    std::vector<CombatEvent> scheduled_events_;
    std::int64_t scheduled_event_count_;
    std::int64_t executed_event_count_;
    std::size_t max_scheduled_queue_size_;
    std::int64_t tick_count_;
    std::uint64_t next_event_sequence_;
    UnitId next_unit_id_;
    TraitRuntimeState trait_runtime_;
    std::vector<ItemEffectGateState> item_effect_gate_state_;
    bool snapshot_recording_;
    std::vector<std::string> snapshots_;
};
