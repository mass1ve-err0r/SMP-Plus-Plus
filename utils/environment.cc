//
// Created by Saadat Baig on 28.06.26.
//
#include "environment.h"

#include <cstdlib>
#include <thread>
#include <stdexcept>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <charconv>
#include <regex>


namespace SMPPlusPlus
{
    // -----------------------------------------------------------------------------
    // Cache singleton
    // -----------------------------------------------------------------------------
    Environment::Cache& Environment::cache() {
        static Cache instance;
        return instance;
    }

    // -----------------------------------------------------------------------------
    // Internal helpers
    // -----------------------------------------------------------------------------
    std::string Environment::requireVar(const char* name) {
        const char* val = std::getenv(name);
        if (!val || val[0] == '\0') {
            std::ostringstream oss;
            oss << "[Env] Required environment variable not set: " << name;
            throw std::runtime_error(oss.str());
        }
        return std::string(val);
    }

    std::optional<std::string> Environment::optionalVar(const char* name) {
        const char* val = std::getenv(name);
        if (!val || val[0] == '\0') return std::nullopt;
        return std::string(val);
    }

    std::string Environment::optionalVar(const char* name, std::string_view defaultValue) {
        const char* val = std::getenv(name);
        if (!val || val[0] == '\0') return std::string(defaultValue);
        return std::string(val);
    }

    uint16_t Environment::optionalVarU16(const char* name, uint16_t defaultValue) {
        const char* val = std::getenv(name);
        if (!val || val[0] == '\0') return defaultValue;
        uint16_t result{};
        auto [ptr, ec] = std::from_chars(val, val + std::strlen(val), result);
        if (ec != std::errc{}) {
            throw std::runtime_error(
                std::string("[Env] Invalid integer value for ") + name + ": " + val);
        }
        return result;
    }

    uint32_t Environment::optionalVarU32(const char* name, uint32_t defaultValue) {
        const char* val = std::getenv(name);
        if (!val || val[0] == '\0') return defaultValue;
        uint32_t result{};
        auto [ptr, ec] = std::from_chars(val, val + std::strlen(val), result);
        if (ec != std::errc{}) {
            throw std::runtime_error(
                std::string("[Env] Invalid integer value for ") + name + ": " + val);
        }
        return result;
    }

    std::vector<uint8_t> Environment::hexDecode(const std::string& hex) {
        if (hex.size() % 2 != 0)
            throw std::runtime_error("[Env] Hex string has odd length");

        std::vector<uint8_t> bytes;
        bytes.reserve(hex.size() / 2);
        for (size_t i = 0; i < hex.size(); i += 2) {
            uint8_t byte{};
            auto [ptr, ec] = std::from_chars(hex.data() + i, hex.data() + i + 2, byte, 16);
            if (ec != std::errc{})
                throw std::runtime_error(
                    std::string("[Env] Invalid hex character at position ") + std::to_string(i));
            bytes.push_back(byte);
        }
        return bytes;
    }

    // -----------------------------------------------------------------------------
    // parseDbUrl
    // Supports: postgresql://user:pass@host:port/dbname
    //           postgresql://user:pass@host/dbname   (port defaults to 5432)
    //           postgresql://host/dbname             (no credentials)
    // -----------------------------------------------------------------------------
    Environment::DbConfig Environment::parseDbUrl(const std::string& url) {
        // Regex: postgresql://[user[:pass]@]host[:port]/dbname
        static const std::regex re(
            R"(^postgresql://(?:([^:@]+)(?::([^@]*))?@)?([^:/]+)(?::(\d+))?/(.+)$)",
            std::regex::ECMAScript
        );

        std::smatch m;
        if (!std::regex_match(url, m, re)) {
            throw std::runtime_error(
                "[Env] SMP_DB_URL is not a valid PostgreSQL URL.\n"
                "  Expected format: postgresql://user:password@host:port/dbname\n"
                "  Got: " + url
            );
        }

        DbConfig cfg;
        cfg.user     = m[1].matched ? m[1].str() : "";
        cfg.password = m[2].matched ? m[2].str() : "";
        cfg.host     = m[3].str();
        cfg.dbname   = m[5].str();

        if (m[4].matched) {
            uint16_t p{};
            const auto& ps = m[4].str();
            auto [ptr, ec] = std::from_chars(ps.data(), ps.data() + ps.size(), p);
            if (ec != std::errc{})
                throw std::runtime_error("[Env] Invalid port in SMP_DB_URL: " + ps);
            cfg.port = p;
        } else {
            cfg.port = 5432;
        }

        // connections per IO thread - can be overridden via SMP_DB_CONNECTIONS
        cfg.connections = 1;
        cfg.timeout     = 30.0;

        return cfg;
    }

    // -----------------------------------------------------------------------------
    // validate()
    // -----------------------------------------------------------------------------
    void Environment::validate() {
        auto& c = cache();
        if (c.validated) return;

        std::vector<std::string> errors;

        auto tryRequire = [&](const char* name, std::string& target) {
            try { target = requireVar(name); }
            catch (const std::runtime_error& e) { errors.emplace_back(e.what()); }
        };

        tryRequire("SMP_MASTER_KEY",     c.masterKey);
        tryRequire("SMP_DB_URL",         c.dbUrl);
        tryRequire("SMP_ADMIN_PASSWORD", c.adminPassword);

        if (!errors.empty()) {
            std::ostringstream oss;
            oss << "[Env] Startup aborted - missing required environment variables:\n";
            for (const auto& err : errors) oss << "  - " << err << "\n";
            throw std::runtime_error(oss.str());
        }

        // Validate master key
        if (c.masterKey.size() != 64)
            throw std::runtime_error(
                "[Env] SMP_MASTER_KEY must be exactly 64 hex characters (32 bytes). "
                "Generate with: openssl rand -hex 32");

        const bool validHex = std::all_of(c.masterKey.begin(), c.masterKey.end(),
            [](char ch) { return std::isxdigit(static_cast<unsigned char>(ch)); });
        if (!validHex)
            throw std::runtime_error("[Env] SMP_MASTER_KEY contains non-hex characters");

        // Parse DB URL
        c.db = parseDbUrl(c.dbUrl);

        // Override connections per thread if set
        c.db.connections = optionalVarU32("SMP_DB_CONNECTIONS", 1);

        // Optional vars
        c.port          = optionalVarU16("SMP_PORT",       443);
        c.logLevel      = optionalVar("SMP_LOG_LEVEL",     "INFO");
        c.tlsCertPath   = optionalVar("SMP_TLS_CERT_PATH", "");
        c.tlsKeyPath    = optionalVar("SMP_TLS_KEY_PATH",  "");
        c.workerThreads = optionalVarU32("SMP_WORKER_THREADS",
                              static_cast<uint32_t>(std::thread::hardware_concurrency()));

        // Dev mode
        {
            std::string val = optionalVar("SMP_DEV_MODE", "false");
            std::transform(val.begin(), val.end(), val.begin(),
                [](unsigned char ch){ return std::tolower(ch); });
            c.devMode = (val == "true");
        }

        c.publicUrl = optionalVar("SMP_PUBLIC_URL", "");
        if (c.publicUrl.empty()) {
            std::cerr << "[Env] WARNING: SMP_PUBLIC_URL not set. "
                         "ServiceGroup hrefs will be incomplete.\n";
        }

        auto sessionSecretOpt = optionalVar("SMP_SESSION_SECRET");
        c.sessionSecret = sessionSecretOpt.value_or(c.masterKey);

        // Validate log level
        const std::vector<std::string_view> validLevels = {
            "TRACE", "DEBUG", "INFO", "WARN", "ERROR"
        };
        if (!std::any_of(validLevels.begin(), validLevels.end(),
                [&](std::string_view lv) { return lv == c.logLevel; }))
            throw std::runtime_error(
                "[Env] SMP_LOG_LEVEL must be one of: TRACE, DEBUG, INFO, WARN, ERROR");

        // TLS consistency check
        if (c.tlsCertPath.empty() != c.tlsKeyPath.empty())
            throw std::runtime_error(
                "[Env] SMP_TLS_CERT_PATH and SMP_TLS_KEY_PATH must both be set or both be empty");

        if (c.tlsCertPath.empty() && !c.devMode)
            std::cerr << "[Env] WARNING: TLS not configured. "
                         "SMP spec requires HTTPS on port 443. "
                         "Only acceptable in development.\n";

        c.validated = true;

        std::cout << "[Env] Configuration validated:\n"
                  << "  SMP_PORT:            " << c.port             << "\n"
                  << "  SMP_LOG_LEVEL:       " << c.logLevel         << "\n"
                  << "  SMP_WORKER_THREADS:  " << c.workerThreads    << "\n"
                  << "  DB host:             " << c.db.host          << "\n"
                  << "  DB port:             " << c.db.port          << "\n"
                  << "  DB name:             " << c.db.dbname        << "\n"
                  << "  DB user:             " << c.db.user          << "\n"
                  << "  DB conns/thread:     " << c.db.connections   << "\n"
                  << "  TLS enabled:         " << (c.tlsCertPath.empty() ? "no" : "yes") << "\n"
                  << "  SMP_DEV_MODE:        " << (c.devMode ? "YES (plain HTTP)" : "no") << "\n"
                  << "  SMP_PUBLIC_URL:      " << (c.publicUrl.empty() ? "[not set]" : c.publicUrl) << "\n"
                  << "  SMP_MASTER_KEY:      [set, not logged]\n"
                  << "  SMP_ADMIN_PASSWORD:  [set, not logged]\n"
                  << "  DB password:         [set, not logged]\n";
    }

    // -----------------------------------------------------------------------------
    // Accessors
    // -----------------------------------------------------------------------------
    static void assertValidated(const Environment::Cache& c) {
        if (!c.validated) [[unlikely]] {
            std::cerr << "[Env] FATAL: Env::validate() must be called before accessing config\n";
            std::abort();
        }
    }

    const std::string& Environment::masterKey() {
        const auto& c = cache(); assertValidated(c); return c.masterKey;
    }
    const std::string& Environment::dbUrl() {
        const auto& c = cache(); assertValidated(c); return c.dbUrl;
    }
    const std::string& Environment::adminPassword() {
        const auto& c = cache(); assertValidated(c); return c.adminPassword;
    }
    uint16_t Environment::port() {
        const auto& c = cache(); assertValidated(c); return c.port;
    }
    const std::string& Environment::logLevel() {
        const auto& c = cache(); assertValidated(c); return c.logLevel;
    }
    const std::string& Environment::sessionSecret() {
        const auto& c = cache(); assertValidated(c); return c.sessionSecret;
    }
    const std::string& Environment::tlsCertPath() {
        const auto& c = cache(); assertValidated(c); return c.tlsCertPath;
    }
    const std::string& Environment::tlsKeyPath() {
        const auto& c = cache(); assertValidated(c); return c.tlsKeyPath;
    }
    uint32_t Environment::workerThreads() {
        const auto& c = cache(); assertValidated(c); return c.workerThreads;
    }
    const std::string& Environment::publicUrl() {
        const auto& c = cache(); assertValidated(c); return c.publicUrl;
    }
    bool Environment::devMode() {
        const auto& c = cache(); assertValidated(c); return c.devMode;
    }
    bool Environment::tlsEnabled() {
        return !tlsCertPath().empty();
    }
    const Environment::DbConfig& Environment::dbConfig() {
        const auto& c = cache(); assertValidated(c); return c.db;
    }
    std::vector<uint8_t> Environment::masterKeyBytes() {
        return hexDecode(masterKey());
    }

} // namespace SMPPlusPlus
