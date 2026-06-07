#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include "core/Board.hpp"
#include "core/Logger.hpp"
#include "core/TeamId.hpp"
#include "combat/TraitRuntime.hpp"
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

    Logger& logger();
    const Logger& logger() const;

    const ContentManager& content() const;

    std::int32_t timeMs() const;
    std::int32_t dtMs() const;
    void setDtMs(std::int32_t dtMs);
    void advanceTick();

    struct ScheduledCombatEvent
    {
        std::int32_t executeAtMs = 0;
        std::function<void()> action{};
        std::string debugName{};
    };

    void scheduleCombatEvent(std::int32_t executeAtMs,
                             std::function<void()> action,
                             std::string debugName);
    void processCombatEvents();

    std::int64_t scheduledEventCount() const;
    std::int64_t executedEventCount() const;
    std::size_t maxScheduledQueueSize() const;
    std::int64_t tickCount() const;

    bool hasAlive(TeamId team) const;

    TraitRuntimeState& traitRuntime();
    const TraitRuntimeState& traitRuntime() const;

private:
    Board board_;
    std::vector<Unit> units_;
    Logger logger_;
    const ContentManager* content_;
    std::int32_t time_ms_;
    std::int32_t dt_ms_;
    std::vector<ScheduledCombatEvent> scheduled_events_;
    std::int64_t scheduled_event_count_;
    std::int64_t executed_event_count_;
    std::size_t max_scheduled_queue_size_;
    std::int64_t tick_count_;
    TraitRuntimeState trait_runtime_;
};
