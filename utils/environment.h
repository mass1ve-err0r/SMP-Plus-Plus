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


namespace SMPPlusPlus {

/**
 * Environment - Single point of access for all environment configuration.
 *
 * Rules:
 *  - Call Environment::validate() once at startup before anything else runs.
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
    // Throws std::runtime_error with a descriptive message on any missing
    // required variable. Logs all resolved values (except secrets) at INFO.
    // -------------------------------------------------------------------------
    static void validate();

    // -------------------------------------------------------------------------
    // Required - application will not start without these
    // -------------------------------------------------------------------------

    /// 32-byte hex-encoded AES-256 master key for encrypting private keys at rest.
    /// Generate with: openssl rand -hex 32
    /// Example: SMP_MASTER_KEY=a3f1...
    static const std::string& masterKey();

    /// PostgreSQL connection string.
    /// Example: SMP_DB_URL=postgresql://user:pass@localhost:5432/smp
    static const std::string& dbUrl();

    /// Initial core admin password. Consumed on first boot to create the admin
    /// user. Ignored on subsequent boots if the admin user already exists.
    /// Example: SMP_ADMIN_PASSWORD=changeme
    static const std::string& adminPassword();

    // -------------------------------------------------------------------------
    // Optional - have sensible defaults
    // -------------------------------------------------------------------------

    /// HTTP/HTTPS port the server listens on. Default: 443
    static uint16_t port();

    /// Log level: TRACE, DEBUG, INFO, WARN, ERROR. Default: INFO
    static const std::string& logLevel();

    /// JWT signing secret for session tokens. Default: derived from masterKey()
    /// Explicitly setting this allows key rotation independent of the master key.
    static const std::string& sessionSecret();

    /// Path to TLS certificate PEM file. Default: "" (TLS disabled - dev only)
    static const std::string& tlsCertPath();

    /// Path to TLS private key PEM file. Default: "" (TLS disabled - dev only)
    static const std::string& tlsKeyPath();

    /// Number of threads in the CPU-bound worker pool (signing, key ops).
    /// Default: std::thread::hardware_concurrency()
    static uint32_t workerThreads();

    /// Maximum PostgreSQL connections in the pool. Default: 20
    static uint32_t dbPoolSize();

    // -------------------------------------------------------------------------
    // Derived helpers - computed from required vars, not from the environment
    // -------------------------------------------------------------------------

    /// Returns the master key as raw bytes (decoded from hex).
    /// Exactly 32 bytes. Throws if masterKey() is not valid 64-char hex.
    static std::vector<uint8_t> masterKeyBytes();

    /// True if TLS is fully configured (both cert and key paths are set).
    static bool tlsEnabled();

private:
    // Cached values - populated by validate()
    struct Cache {
        // Required
        std::string masterKey;
        std::string dbUrl;
        std::string adminPassword;

        // Optional
        uint16_t    port           = 443;
        std::string logLevel       = "INFO";
        std::string sessionSecret;
        std::string tlsCertPath;
        std::string tlsKeyPath;
        uint32_t    workerThreads  = 0;   // 0 = use hardware_concurrency
        uint32_t    dbPoolSize     = 20;

        bool validated = false;
    };

    static Cache& cache();

    // Internal helpers
    static std::string requireVar(const char* name);
    static std::optional<std::string> optionalVar(const char* name);
    static std::string optionalVar(const char* name, std::string_view defaultValue);
    static uint16_t optionalVarU16(const char* name, uint16_t defaultValue);
    static uint32_t optionalVarU32(const char* name, uint32_t defaultValue);
    static std::vector<uint8_t> hexDecode(const std::string& hex);
};

} // namespace smp
