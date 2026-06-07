#include "core/Json.hpp"
#include <cctype>
#include <cstdlib>
#include <stdexcept>

static std::runtime_error makeError(std::string_view message, std::size_t pos)
{
    std::string s;
    s.reserve(message.size() + 32);
    s.append(message.begin(), message.end());
    s.append(" at pos ");
    s.append(std::to_string(pos));
    return std::runtime_error(s);
}

bool JsonValue::isNull() const { return std::holds_alternative<std::nullptr_t>(value); }
bool JsonValue::isBool() const { return std::holds_alternative<bool>(value); }
bool JsonValue::isNumber() const { return std::holds_alternative<double>(value); }
bool JsonValue::isString() const { return std::holds_alternative<std::string>(value); }
bool JsonValue::isArray() const { return std::holds_alternative<Array>(value); }
bool JsonValue::isObject() const { return std::holds_alternative<Object>(value); }

const bool& JsonValue::asBool() const { return std::get<bool>(value); }
const double& JsonValue::asNumber() const { return std::get<double>(value); }
const std::string& JsonValue::asString() const { return std::get<std::string>(value); }
const JsonValue::Array& JsonValue::asArray() const { return std::get<Array>(value); }
const JsonValue::Object& JsonValue::asObject() const { return std::get<Object>(value); }

std::string JsonValue::dumpType() const
{
    if (isNull()) return "null";
    if (isBool()) return "bool";
    if (isNumber()) return "number";
    if (isString()) return "string";
    if (isArray()) return "array";
    if (isObject()) return "object";
    return "unknown";
}

bool JsonValue::hasKey(std::string_view key) const
{
    if (!isObject()) return false;
    const auto& obj = asObject();
    return obj.find(std::string(key)) != obj.end();
}

const JsonValue& JsonValue::at(std::string_view key) const
{
    const auto& obj = asObject();
    auto it = obj.find(std::string(key));
    if (it == obj.end())
    {
        throw std::runtime_error(std::string("Missing key: ") + std::string(key));
    }
    return it->second;
}

JsonParser::JsonParser(std::string_view input)
    : input_(input), pos_(0)
{
}

void JsonParser::skipWhitespace()
{
    while (pos_ < input_.size() &&
           std::isspace(static_cast<unsigned char>(input_[pos_])) != 0)
    {
        ++pos_;
    }
}

bool JsonParser::consume(char c)
{
    skipWhitespace();
    if (pos_ < input_.size() && input_[pos_] == c)
    {
        ++pos_;
        return true;
    }
    return false;
}

char JsonParser::peek() const
{
    if (pos_ >= input_.size()) return '\0';
    return input_[pos_];
}

char JsonParser::get()
{
    if (pos_ >= input_.size())
    {
        fail("Unexpected end of input");
    }
    return input_[pos_++];
}

[[noreturn]] void JsonParser::fail(std::string_view message) const
{
    throw makeError(message, pos_);
}

JsonValue JsonParser::parse()
{
    skipWhitespace();
    JsonValue v = parseValue();
    skipWhitespace();

    if (pos_ != input_.size())
    {
        fail("Trailing characters");
    }

    return v;
}

JsonValue JsonParser::parseValue()
{
    skipWhitespace();

    const char c = peek();

    if (c == 'n') return parseNull();
    if (c == 't' || c == 'f') return parseBool();
    if (c == '"') return parseString();
    if (c == '[') return parseArray();
    if (c == '{') return parseObject();
    if (c == '-' || (c >= '0' && c <= '9')) return parseNumber();

    fail("Invalid value");
}

JsonValue JsonParser::parseNull()
{
    if (input_.substr(pos_, 4) != "null")
    {
        fail("Invalid null");
    }

    pos_ += 4;
    return JsonValue{nullptr};
}

JsonValue JsonParser::parseBool()
{
    if (input_.substr(pos_, 4) == "true")
    {
        pos_ += 4;
        return JsonValue{true};
    }

    if (input_.substr(pos_, 5) == "false")
    {
        pos_ += 5;
        return JsonValue{false};
    }

    fail("Invalid bool");
}

JsonValue JsonParser::parseNumber()
{
    skipWhitespace();

    const std::size_t start = pos_;

    if (peek() == '-')
    {
        ++pos_;
    }

    if (peek() == '0')
    {
        ++pos_;
    }
    else
    {
        if (!(peek() >= '1' && peek() <= '9'))
        {
            fail("Invalid number");
        }

        while (peek() >= '0' && peek() <= '9')
        {
            ++pos_;
        }
    }

    if (peek() == '.')
    {
        ++pos_;

        if (!(peek() >= '0' && peek() <= '9'))
        {
            fail("Invalid fraction");
        }

        while (peek() >= '0' && peek() <= '9')
        {
            ++pos_;
        }
    }

    if (peek() == 'e' || peek() == 'E')
    {
        ++pos_;

        if (peek() == '+' || peek() == '-')
        {
            ++pos_;
        }

        if (!(peek() >= '0' && peek() <= '9'))
        {
            fail("Invalid exponent");
        }

        while (peek() >= '0' && peek() <= '9')
        {
            ++pos_;
        }
    }

    const std::string_view numStr = input_.substr(start, pos_ - start);
    std::string tmp(numStr);

    char* endPtr = nullptr;
    const double out = std::strtod(tmp.c_str(), &endPtr);

    if (endPtr == tmp.c_str())
    {
        fail("Failed to parse number");
    }

    return JsonValue{out};
}

std::string JsonParser::parseStringRaw()
{
    if (get() != '"')
    {
        fail("Expected string");
    }

    std::string out;

    while (true)
    {
        const char c = get();

        if (c == '"')
        {
            break;
        }

        if (c == '\\')
        {
            const char e = get();

            switch (e)
            {
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case '/': out.push_back('/'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;

                case 'u':
                    fail("Unicode escapes not supported");

                default:
                    fail("Invalid escape");
            }
        }
        else
        {
            out.push_back(c);
        }
    }

    return out;
}

JsonValue JsonParser::parseString()
{
    return JsonValue{parseStringRaw()};
}

JsonValue JsonParser::parseArray()
{
    if (get() != '[')
    {
        fail("Expected '['");
    }

    JsonValue::Array arr;

    skipWhitespace();

    if (consume(']'))
    {
        return JsonValue{std::move(arr)};
    }

    while (true)
    {
        arr.push_back(parseValue());

        skipWhitespace();

        if (consume(']'))
        {
            break;
        }

        if (!consume(','))
        {
            fail("Expected ',' or ']'");
        }
    }

    return JsonValue{std::move(arr)};
}

JsonValue JsonParser::parseObject()
{
    if (get() != '{')
    {
        fail("Expected '{'");
    }

    JsonValue::Object obj;

    skipWhitespace();

    if (consume('}'))
    {
        return JsonValue{std::move(obj)};
    }

    while (true)
    {
        skipWhitespace();

        if (peek() != '"')
        {
            fail("Expected object key string");
        }

        std::string key = parseStringRaw();

        skipWhitespace();

        if (!consume(':'))
        {
            fail("Expected ':'");
        }

        JsonValue val = parseValue();

        obj.emplace(std::move(key), std::move(val));

        skipWhitespace();

        if (consume('}'))
        {
            break;
        }

        if (!consume(','))
        {
            fail("Expected ',' or '}'");
        }
    }

    return JsonValue{std::move(obj)};
}

JsonValue parseJson(std::string_view input)
{
    JsonParser p(input);
    return p.parse();
}