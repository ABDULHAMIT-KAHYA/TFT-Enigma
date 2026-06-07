#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

struct JsonValue
{
    using Array = std::vector<JsonValue>;
    using Object = std::unordered_map<std::string, JsonValue>;

    std::variant<std::nullptr_t, bool, double, std::string, Array, Object> value;

    JsonValue() : value(nullptr) {}
    JsonValue(std::nullptr_t v) : value(v) {}
    JsonValue(bool v) : value(v) {}
    JsonValue(double v) : value(v) {}
    JsonValue(std::string v) : value(std::move(v)) {}
    JsonValue(Array v) : value(std::move(v)) {}
    JsonValue(Object v) : value(std::move(v)) {}

    bool isNull() const;
    bool isBool() const;
    bool isNumber() const;
    bool isString() const;
    bool isArray() const;
    bool isObject() const;

    const bool& asBool() const;
    const double& asNumber() const;
    const std::string& asString() const;
    const Array& asArray() const;
    const Object& asObject() const;

    std::string dumpType() const;
    bool hasKey(std::string_view key) const;
    const JsonValue& at(std::string_view key) const;
};

class JsonParser
{
public:
    explicit JsonParser(std::string_view input);

    JsonValue parse();

private:
    std::string_view input_;
    std::size_t pos_;

    void skipWhitespace();
    bool consume(char c);
    char peek() const;
    char get();

    [[noreturn]] void fail(std::string_view message) const;

    JsonValue parseValue();
    JsonValue parseNull();
    JsonValue parseBool();
    JsonValue parseNumber();
    JsonValue parseString();
    JsonValue parseArray();
    JsonValue parseObject();

    std::string parseStringRaw();
};

JsonValue parseJson(std::string_view input);