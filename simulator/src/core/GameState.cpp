#include "core/GameState.hpp"
#include "content/ContentManager.hpp"
#include "constants/GameConstants.hpp"
#include <algorithm>
#include <sstream>
#include <utility>
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
      tick_count_(0)
{
}

Board& GameState::board()
{
    return board_;
}

const Board& GameState::board() const
{
    return board_;
}

std::vector<Unit>& GameState::units()
{
    return units_;
}

const std::vector<Unit>& GameState::units() const
{
    return units_;
}

Logger& GameState::logger()
{
    return logger_;
}

const Logger& GameState::logger() const
{
    return logger_;
}

const ContentManager& GameState::content() const
{
    return *content_;
}

std::int32_t GameState::timeMs() const
{
    return time_ms_;
}

std::int32_t GameState::dtMs() const
{
    return dt_ms_;
}

void GameState::setDtMs(std::int32_t dtMs)
{
    dt_ms_ = dtMs;
}

void GameState::advanceTick()
{
    time_ms_ += dt_ms_;
    tick_count_ += 1;
}

void GameState::scheduleCombatEvent(std::int32_t executeAtMs,
                                    std::function<void()> action,
                                    std::string debugName)
{
    ScheduledCombatEvent e{};
    e.executeAtMs = executeAtMs;
    e.action = std::move(action);
    e.debugName = std::move(debugName);
    scheduled_events_.push_back(std::move(e));
    scheduled_event_count_ += 1;
    max_scheduled_queue_size_ = std::max(max_scheduled_queue_size_, scheduled_events_.size());

    std::ostringstream ss;
    ss << time_ms_ << "ms Scheduled " << scheduled_events_.back().debugName
       << " at " << executeAtMs << "ms";
    logger_.combat(ss.str());
}

void GameState::processCombatEvents()
{
    if (scheduled_events_.empty())
    {
        return;
    }

    std::stable_sort(
        scheduled_events_.begin(),
        scheduled_events_.end(),
        [](const ScheduledCombatEvent& a, const ScheduledCombatEvent& b)
        {
            return a.executeAtMs < b.executeAtMs;
        }
    );

    std::size_t i = 0;
    for (; i < scheduled_events_.size(); ++i)
    {
        if (scheduled_events_[i].executeAtMs > time_ms_)
        {
            break;
        }
        std::ostringstream ss;
        ss << time_ms_ << "ms Executing " << scheduled_events_[i].debugName;
        logger_.combat(ss.str());
        executed_event_count_ += 1;

        if (scheduled_events_[i].action)
        {
            scheduled_events_[i].action();
        }
    }

    if (i > 0)
    {
        scheduled_events_.erase(
            scheduled_events_.begin(),
            scheduled_events_.begin() + static_cast<std::ptrdiff_t>(i)
        );
    }
}

std::int64_t GameState::scheduledEventCount() const
{
    return scheduled_event_count_;
}

std::int64_t GameState::executedEventCount() const
{
    return executed_event_count_;
}

std::size_t GameState::maxScheduledQueueSize() const
{
    return max_scheduled_queue_size_;
}

std::int64_t GameState::tickCount() const
{
    return tick_count_;
}

bool GameState::hasAlive(TeamId team) const
{
    for (const Unit& unit : units_)
    {
        if (unit.getTeamId() == team && unit.isAlive())
        {
            return true;
        }
    }
    return false;
}

TraitRuntimeState& GameState::traitRuntime()
{
    return trait_runtime_;
}

const TraitRuntimeState& GameState::traitRuntime() const
{
    return trait_runtime_;
}
