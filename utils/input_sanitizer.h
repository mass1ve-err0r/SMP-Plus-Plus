//
// Created by Saadat Baig on 07.07.26.
//
#pragma once

#include <json/json.h>
#include <string>
#include <string_view>
#include <optional>


namespace SMPPlusPlus
{
    /**
     * InputSanitizer - Global forbidden-character check for user input.
     *
     * Stateless, thread-safe, concurrency-friendly. All methods are static
     * and operate on their arguments only - no shared mutable state.
     *
     * Forbidden character set (globally rejected on any field this validates):
     *   ';'  - SQL/statement terminator; never legitimate in any field we accept
     *   '<'  - XSS vector; never legitimate in Peppol data or PEMs
     *   '>'  - XSS vector; never legitimate in Peppol data or PEMs
     *   '\\' - escape character; never legitimate (URLs use '/', PEMs use '-')
     *   '\0' - null byte; never valid input
     *
     * NOT forbidden (legitimate in various fields):
     *   '  \"        - legitimate in descriptions and quoted strings
     *   :           - Peppol identifier separator
     *   / + =       - base64 charset (PEM content)
     *   - _         - base64url + PEM boundaries
     *   \n \r \t    - whitespace, required in PEMs and multi-line fields
     *
     * We use parameterized SQL queries throughout, so this is defense-in-depth
     * against XSS and malformed data, not primary SQL injection protection.
     */
    class InputSanitizer {
    public:
        InputSanitizer() = delete;

        /// Character forbidden anywhere.
        /// Returns true if the input contains any forbidden character.
        static bool containsForbidden(std::string_view input) noexcept;

        /// Returns the offending character (as a printable string) if any,
        /// otherwise nullopt. Useful for error messages.
        static std::optional<std::string> firstForbidden(
            std::string_view input) noexcept;

        /// Convenience: returns true if the string is safe (no forbidden chars).
        static bool isSafe(std::string_view input) noexcept {
            return !containsForbidden(input);
        }

        /// Validate every string value in a JSON object recursively.
        /// Returns the JSON path (e.g. "processes[0].endpoints[1].endpoint_url")
        /// of the first offending value, or nullopt if all clean.
        /// Useful for one-shot creation payloads with nested structures.
        ///
        /// The `skipKeys` set names keys whose values are exempt from the check
        /// (e.g. cert_pem / private_key_pem where the base64 body could span
        /// many lines and is validated by CryptoUtils separately).
        struct JsonViolation {
            std::string path;
            std::string forbiddenChar;
        };

        static std::optional<JsonViolation> validateJson(
            const Json::Value& value,
            const std::initializer_list<std::string_view>& skipKeys = {}
        ) noexcept;

    private:
        static std::string charToDisplay(char c) noexcept;
    };

} // namespace SMPPlusPlus
