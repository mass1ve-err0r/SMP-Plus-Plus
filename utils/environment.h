//
// Created by Saadat Baig on 28.06.26.
//
#pragma once

#include <string>
#include <string_view>
#include <cstdint>
#include <vector>
#include <stdexcept>
#include <optional>


namespace SMPPlusPlus
{
    /**
     * Env - Single point of access for all environment configuration.
     *
     * Rules:
     *  - Call Env::validate() once at startup before anything else runs.
     *  - Required vars: missing = immediate abort with a clear message.
     *  - Optional vars: return typed defaults if not set.
     *  - Never call getenv() anywhere else in the codebase - go through here.
     *  - All values are read once at validate() and cached. No mid-run re-reads.
     */
    class Environment {
    public:
        Environment() = delete;

        // -------------------------------------------------------------------------
        // Startup validation - call exactly once from main() before app.run()
        // -------------------------------------------------------------------------
        static void validate();

        // -------------------------------------------------------------------------
        // Required
        // -------------------------------------------------------------------------

        /// 32-byte hex-encoded AES-256 master key for encrypting private keys at rest.
        /// Generate with: openssl rand -hex 32
        static const std::string& masterKey();

        /// PostgreSQL connection URL.
        /// Example: SMP_DB_URL=postgresql://user:pass@localhost:5432/smp
        static const std::string& dbUrl();

        /// Initial core admin password. Consumed on first boot.
        /// Ignored on subsequent boots if the admin user already exists.
        static const std::string& adminPassword();

        // -------------------------------------------------------------------------
        // Optional
        // -------------------------------------------------------------------------

        /// HTTP/HTTPS port. Default: 443
        static uint16_t port();

        /// Log level: TRACE, DEBUG, INFO, WARN, ERROR. Default: INFO
        static const std::string& logLevel();

        /// JWT signing secret. Default: derived from masterKey()
        static const std::string& sessionSecret();

        /// Path to TLS certificate PEM file. Default: ""
        static const std::string& tlsCertPath();

        /// Path to TLS private key PEM file. Default: ""
        static const std::string& tlsKeyPath();

        /// Number of IO threads. Default: hardware_concurrency()
        static uint32_t workerThreads();

        /// Public base URL of this SMP instance.
        /// Used to construct ServiceMetadataReference hrefs in ServiceGroup responses.
        /// Example: SMP_PUBLIC_URL=https://my.smp.com
        static const std::string& publicUrl();

        /// Dev mode: plain HTTP, no TLS required.
        /// Set SMP_DEV_MODE=true for local development only.
        /// Any value other than "true" (case-insensitive) is treated as false.
        static bool devMode();

        // -------------------------------------------------------------------------
        // Derived
        // -------------------------------------------------------------------------

        /// True if TLS is fully configured (both cert and key paths are set).
        static bool tlsEnabled();

        /// Returns the master key as raw bytes (decoded from hex). Exactly 32 bytes.
        static std::vector<uint8_t> masterKeyBytes();

        // -------------------------------------------------------------------------
        // DB configuration - parsed once from SMP_DB_URL during validate()
        // -------------------------------------------------------------------------
        struct DbConfig {
            std::string host;
            uint16_t    port        = 5432;
            std::string dbname;
            std::string user;
            std::string password;
            uint32_t    connections = 1;    // per IO thread (is_fast mode)
            double      timeout     = 30.0;
        };

        static const DbConfig& dbConfig();

        struct Cache {
            // Required
            std::string masterKey;
            std::string dbUrl;
            std::string adminPassword;

            // Optional
            uint16_t    port          = 10420;
            std::string logLevel      = "INFO";
            std::string sessionSecret;
            std::string tlsCertPath;
            std::string tlsKeyPath;
            uint32_t    workerThreads = 0;
            bool        devMode       = true;
            std::string publicUrl;

            // Parsed from dbUrl
            DbConfig    db;

            bool validated = false;
        };

        static Cache& cache();

    private:

        static std::string requireVar(const char* name);
        static std::optional<std::string> optionalVar(const char* name);
        static std::string optionalVar(const char* name, std::string_view defaultValue);
        static uint16_t optionalVarU16(const char* name, uint16_t defaultValue);
        static uint32_t optionalVarU32(const char* name, uint32_t defaultValue);
        static std::vector<uint8_t> hexDecode(const std::string& hex);
        static DbConfig parseDbUrl(const std::string& url);
    };

} // namespace SMPPlusPlus
