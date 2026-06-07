#pragma once

#include <cstdint>
#include <ostream>
#include <string_view>

enum class LogMode
{
    Silent,
    Normal,
    Verbose
};

class Logger
{
public:
    explicit Logger(std::ostream& out);

    void setMode(LogMode mode);
    LogMode mode() const;

    void info(std::string_view message);
    void move(std::string_view message);
    void combat(std::string_view message);
    void warn(std::string_view message);
    void error(std::string_view message);

private:
    bool enabledFor(LogMode required) const;

    std::ostream* out_;
    LogMode mode_;
};
