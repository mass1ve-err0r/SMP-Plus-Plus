//
// Created by Saadat Baig on 29.06.26.
//
#include "url_utils.h"

#include <sstream>
#include <iomanip>
#include <cctype>
#include <charconv>


namespace SMPPlusPlus
{
    std::string
    UrlUtils::percentEncode(std::string_view input)
    {
        std::ostringstream oss;
        oss << std::hex << std::uppercase;

        for (unsigned char c : input)
        {
            // RFC 3986 unreserved characters - not encoded
            if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
            {
                oss << static_cast<char>(c);
            } else
            {
                oss << '%' << std::setw(2) << std::setfill('0') << static_cast<unsigned>(c);
            }
        }

        return oss.str();
    }

    std::optional<std::string>
    UrlUtils::percentDecode(std::string_view input)
    {
        std::string result;
        result.reserve(input.size());

        for (size_t i = 0; i < input.size(); ++i)
        {
            if (input[i] == '%')
            {
                if (i + 2 >= input.size()) return std::nullopt;

                uint8_t byte{};
                auto [ptr, ec] = std::from_chars(
                    input.data() + i + 1,
                    input.data() + i + 3,
                    byte, 16
                );
                if (ec != std::errc{}) return std::nullopt;

                result += static_cast<char>(byte);
                i += 2;
            } else if (input[i] == '+')
            {
                result += ' ';
            } else
            {
                result += input[i];
            }
        }

        return result;
    }

    std::optional<UrlUtils::PeppolIdentifier>
    UrlUtils::parsePeppolId(std::string_view encoded)
    {
        auto decoded = percentDecode(encoded);
        if (!decoded) return std::nullopt;

        // Split on "::" separator
        const auto sep = decoded->find("::");
        if (sep == std::string::npos) return std::nullopt;

        return PeppolIdentifier{
            decoded->substr(0, sep),
            decoded->substr(sep + 2)
        };
    }

    std::string
    UrlUtils::encodePeppolId(std::string_view scheme, std::string_view value)
    {
        // Encode scheme::value — the "::" and any ":" in value get encoded
        std::string combined;
        combined.reserve(scheme.size() + 2 + value.size());
        combined += scheme;
        combined += "::";
        combined += value;

        return percentEncode(combined);
    }

} // namespace SMPPlusPlus
