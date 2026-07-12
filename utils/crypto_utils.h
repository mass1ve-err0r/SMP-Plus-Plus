//
// Created by Saadat Baig on 28.06.26.
//
#pragma once

#include <tl/expected.hpp>

#include <string>
#include <string_view>
#include <vector>
#include <cstdint>
#include <span>

#include "utils/typedefs.h"


namespace SMPPlusPlus
{
    /**
     * CryptoUtils - Stateless cryptographic utilities.
     *
     * All functions are thread-safe. No shared mutable state.
     * Uses OpenSSL EVP_* API throughout (OpenSSL >= 1.1.0 required).
     *
     * Threading rules:
     *   - All methods are safe to call concurrently from multiple threads.
     *   - bcryptHash() and bcryptVerify() are CPU-intensive (~100ms at cost 12).
     *     NEVER call these on a Drogon IO thread directly.
     *     Always offload via drogon::async_run() or equivalent.
     *
     * Private key storage model:
     *   - Private key PEM encrypted with AES-256-GCM
     *   - Key = Env::masterKeyBytes() (32 bytes from SMP_MASTER_KEY)
     *   - IV  = random 12 bytes per encryption
     *   - Tag = 16-byte GCM authentication tag
     *   - Stored in DB as: base64(iv).base64(tag).base64(ciphertext)
     */
    class CryptoUtils
    {
    public:

        CryptoUtils() = delete;

        // -------------------------------------------------------------------------
        // Password hashing - bcrypt
        //
        // WARNING: These are blocking, CPU-bound operations (~100ms at workFactor=12).
        // Always call from a worker thread, never from a Drogon IO thread.
        //
        // Example (inside a Drogon coroutine handler):
        //   auto result = co_await drogon::async_run([&]() {
        //       return CryptoUtils::bcryptHash(password);
        //   });
        // -------------------------------------------------------------------------

        /**
         * Hash a plaintext password with bcrypt.
         *
         * @param password    Plaintext password (max 72 bytes - bcrypt limit)
         * @param workFactor  Cost factor 4-31. 12 = ~250ms on modern hardware.
         *                    Higher = slower = harder to brute-force.
         *                    Never go below 10 in production.
         * @return            bcrypt hash string, e.g. "$2b$12$..."
         *                    Store this entire string in the DB - it contains
         *                    the salt and cost factor, nothing else needed.
         */
        static Result<std::string>
        bcryptHash(std::string_view password, int workFactor = 12);

        /**
         * Verify a plaintext password against a stored bcrypt hash.
         *
         * @param password    Plaintext password to verify
         * @param hash        Stored bcrypt hash from bcryptHash()
         * @return            true if password matches, false if not.
         *                    Returns Error only on malformed hash input,
         *                    not on password mismatch (mismatch = false).
         *
         * Timing: constant-time comparison is handled internally by bcrypt.
         */
        static Result<bool>
        bcryptVerify(std::string_view password, std::string_view hash);

        // -------------------------------------------------------------------------
        // AES-256-GCM (encrypting/decrypting PEM private keys at rest)
        // -------------------------------------------------------------------------

        static Result<std::string>
        aesEncrypt(std::string_view plaintext, std::span<const uint8_t> keyBytes);

        static Result<std::string>
        aesDecrypt(std::string_view encoded, std::span<const uint8_t> keyBytes);

        // -------------------------------------------------------------------------
        // Base64
        // -------------------------------------------------------------------------
        static std::string
        base64Encode(std::span<const uint8_t> data);

        static std::string
        base64Encode(std::string_view data);

        static Result<std::vector<uint8_t>>
        base64Decode(std::string_view encoded);

        // -------------------------------------------------------------------------
        // Hex
        // -------------------------------------------------------------------------
        static std::string
        hexEncode(std::span<const uint8_t> data);

        static Result<std::vector<uint8_t>>
        hexDecode(std::string_view hex);

        // -------------------------------------------------------------------------
        // Random bytes
        // -------------------------------------------------------------------------
        static Result<std::vector<uint8_t>>
        randomBytes(size_t count);

        // -------------------------------------------------------------------------
        // Certificate utilities
        // -------------------------------------------------------------------------
        static Result<std::string>
        certFingerprint(std::string_view pemCert);

        static Result<std::string>
        certSubjectUID(std::string_view pemCert);

        struct CertValidity
        {
            std::string notBefore;
            std::string notAfter;
        };

        static Result<CertValidity>
        certValidity(std::string_view pemCert);

        static Result<void>
        verifyCertKeyPair(std::string_view pemCert, std::string_view pemPrivateKey);

    private:

        static constexpr char   BLOB_SEPARATOR = '.';
        static constexpr size_t IV_SIZE        = 12;
        static constexpr size_t TAG_SIZE       = 16;

        // bcrypt max password length - hard limit of the algorithm
        static constexpr size_t BCRYPT_MAX_PASSWORD_LEN = 72;

    };

} // namespace SMPPlusPlus
