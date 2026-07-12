//
// Created by Saadat Baig on 07.07.26.
//
#include "input_sanitizer.h"

#include <json/json.h>

#include <array>
#include <sstream>


namespace SMPPlusPlus
{
    namespace
    {
        // Forbidden characters - defined in one place, referenced by all methods.
        // Static constexpr = zero runtime cost, no shared mutable state.
        constexpr std::array<char, 5> FORBIDDEN = {
            ';',   // statement terminator
            '<',   // XSS vector
            '>',   // XSS vector
            '\\',  // escape character
            '\0'   // null byte
        };

        constexpr bool
        isForbidden(char c) noexcept {
            for (char f : FORBIDDEN)
            {
                if (c == f) return true;
            }

            return false;
        }

        // Recursive JSON walker - stateless, thread-safe.
        // Returns a JsonViolation on the first offending string value.
        std::optional<InputSanitizer::JsonViolation>
        walkJson(const Json::Value& value, const std::string& path, const std::initializer_list<std::string_view>& skipKeys) noexcept
        {
            if (value.isString())
            {
                const std::string& s = value.asString();
                for (char c : s)
                {
                    if (isForbidden(c))
                    {
                        std::string display;
                        switch (c)
                        {
                            case '\0': display = "\\0";  break;
                            case '\\': display = "\\\\"; break;
                            default:   display = std::string(1, c); break;
                        }
                        return InputSanitizer::JsonViolation{path, display};
                    }
                }

                return std::nullopt;
            }

            if (value.isObject())
            {
                for (const auto& key : value.getMemberNames())
                {
                    // Skip exempt keys (e.g. PEM fields)
                    bool skip = false;
                    for (auto sk : skipKeys)
                    {
                        if (key == sk) { skip = true; break; }
                    }
                    if (skip) continue;

                    const std::string childPath = path.empty() ? key : (path + "." + key);
                    auto v = walkJson(value[key], childPath, skipKeys);
                    if (v) return v;
                }

                return std::nullopt;
            }

            if (value.isArray())
            {
                for (Json::ArrayIndex i = 0; i < value.size(); ++i)
                {
                    const std::string childPath = path + "[" + std::to_string(i) + "]";
                    auto v = walkJson(value[i], childPath, skipKeys);
                    if (v) return v;
                }

                return std::nullopt;
            }

            // Numbers, bools, null - nothing to sanitize
            return std::nullopt;
        }
    } // anonymous namespace

    bool
    InputSanitizer::containsForbidden(std::string_view input) noexcept
    {
        for (char c : input)
        {
            if (isForbidden(c)) return true;
        }

        return false;
    }

    std::optional<std::string>
    InputSanitizer::firstForbidden(std::string_view input) noexcept
    {
        for (char c : input)
        {
            if (isForbidden(c)) return charToDisplay(c);
        }

        return std::nullopt;
    }

    std::optional<InputSanitizer::JsonViolation>
    InputSanitizer::validateJson(const Json::Value& value, const std::initializer_list<std::string_view>& skipKeys) noexcept
    {
        return walkJson(value, "", skipKeys);
    }

    std::string
    InputSanitizer::charToDisplay(char c) noexcept
    {
        switch (c)
        {
            case '\0': return "\\0";
            case '\\': return "\\\\";
            default:   return std::string(1, c);
        }
    }

} // namespace SMPPlusPlus
