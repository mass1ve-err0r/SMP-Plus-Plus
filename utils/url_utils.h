//
// Created by Saadat Baig on 29.06.26.
//
#pragma once

#include <string>
#include <string_view>
#include <optional>


namespace SMPPlusPlus
{
    /**
     * UrlUtils - Percent-encoding utilities for Peppol identifiers in URLs.
     *
     * Peppol identifiers (e.g. "iso6523-actorid-upis::0088:123456789") must be
     * percent-encoded when used in URLs. The "::" separator becomes "%3A%3A"
     * and ":" within the value becomes "%3A".
     *
     * All functions are stateless and thread-safe.
     */
    class UrlUtils
    {
    public:

        UrlUtils() = delete;

        /// Percent-encode a string per RFC 3986.
        /// Unreserved characters (A-Z a-z 0-9 - _ . ~) are not encoded.
        static std::string
        percentEncode(std::string_view input);

        /// Decode a percent-encoded string.
        /// Returns nullopt if the input contains malformed percent sequences.
        static std::optional<std::string>
        percentDecode(std::string_view input);

        /**
         * Parse a Peppol identifier from a URL path segment.
         * Expects percent-encoded "scheme%3A%3Avalue" format.
         * Returns {scheme, value} or nullopt if malformed.
         *
         * Example:
         *   "iso6523-actorid-upis%3A%3A0088%3A123456789"
         *   -> { "iso6523-actorid-upis", "0088:123456789" }
         */
        struct PeppolIdentifier
        {
            std::string scheme;
            std::string value;
        };

        static std::optional<PeppolIdentifier>
        parsePeppolId(std::string_view encoded);

        /**
         * Encode a Peppol identifier for use in a URL path segment.
         * "iso6523-actorid-upis::0088:123456789"
         * -> "iso6523-actorid-upis%3A%3A0088%3A123456789"
         */
        static std::string
        encodePeppolId(std::string_view scheme, std::string_view value);

    };

} // namespace SMPPlusPlus
