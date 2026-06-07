#include "core/Logger.hpp"
Logger::Logger(std::ostream& out)
    : out_(&out),
      mode_(LogMode::Verbose)
{
}

void Logger::setMode(LogMode mode)
{
    mode_ = mode;
}

LogMode Logger::mode() const
{
    return mode_;
}

bool Logger::enabledFor(LogMode required) const
{
    return static_cast<std::int32_t>(mode_) >= static_cast<std::int32_t>(required);
}

void Logger::info(std::string_view message)
{
    if (!enabledFor(LogMode::Normal))
    {
        return;
    }
    (*out_) << message << "\n";
}

void Logger::move(std::string_view message)
{
    if (!enabledFor(LogMode::Verbose))
    {
        return;
    }
    (*out_) << message << "\n";
}

void Logger::combat(std::string_view message)
{
    if (!enabledFor(LogMode::Normal))
    {
        return;
    }
    (*out_) << message << "\n";
}

void Logger::warn(std::string_view message)
{
    if (!enabledFor(LogMode::Normal))
    {
        return;
    }
    (*out_) << message << "\n";
}

void Logger::error(std::string_view message)
{
    (*out_) << message << "\n";
}
