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


namespace SMPPlusPlus {

// -----------------------------------------------------------------------------
// Cache singleton - lives for the duration of the process
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
        std::ostringstream oss;
        oss << "[Env] Invalid integer value for " << name << ": " << val;
        throw std::runtime_error(oss.str());
    }
    return result;
}

uint32_t Environment::optionalVarU32(const char* name, uint32_t defaultValue) {
    const char* val = std::getenv(name);
    if (!val || val[0] == '\0') return defaultValue;

    uint32_t result{};
    auto [ptr, ec] = std::from_chars(val, val + std::strlen(val), result);
    if (ec != std::errc{}) {
        std::ostringstream oss;
        oss << "[Env] Invalid integer value for " << name << ": " << val;
        throw std::runtime_error(oss.str());
    }
    return result;
}

std::vector<uint8_t> Environment::hexDecode(const std::string& hex) {
    if (hex.size() % 2 != 0) {
        throw std::runtime_error("[Env] Hex string has odd length - not valid hex");
    }

    std::vector<uint8_t> bytes;
    bytes.reserve(hex.size() / 2);

    for (size_t i = 0; i < hex.size(); i += 2) {
        uint8_t byte{};
        auto [ptr, ec] = std::from_chars(hex.data() + i, hex.data() + i + 2, byte, 16);
        if (ec != std::errc{}) {
            std::ostringstream oss;
            oss << "[Env] Invalid hex character at position " << i;
            throw std::runtime_error(oss.str());
        }
        bytes.push_back(byte);
    }
    return bytes;
}

// -----------------------------------------------------------------------------
// validate() - call once from main()
// -----------------------------------------------------------------------------
void Environment::validate() {
    auto& c = cache();
    if (c.validated) return; // Idempotent

    std::vector<std::string> errors;

    // --- Required vars ---
    auto tryRequire = [&](const char* name, std::string& target) {
        try {
            target = requireVar(name);
        } catch (const std::runtime_error& e) {
            errors.emplace_back(e.what());
        }
    };

    tryRequire("SMP_MASTER_KEY",     c.masterKey);
    tryRequire("SMP_DB_URL",         c.dbUrl);
    tryRequire("SMP_ADMIN_PASSWORD", c.adminPassword);

    if (!errors.empty()) {
        std::ostringstream oss;
        oss << "[Env] Startup aborted - missing required environment variables:\n";
        for (const auto& err : errors) {
            oss << "  - " << err << "\n";
        }
        throw std::runtime_error(oss.str());
    }

    // --- Validate master key is 64-char hex (= 32 bytes) ---
    if (c.masterKey.size() != 64) {
        throw std::runtime_error(
            "[Env] SMP_MASTER_KEY must be exactly 64 hex characters (32 bytes). "
            "Generate with: openssl rand -hex 32"
        );
    }
    {
        const bool validHex = std::all_of(c.masterKey.begin(), c.masterKey.end(),
            [](char ch) { return std::isxdigit(static_cast<unsigned char>(ch)); });
        if (!validHex) {
            throw std::runtime_error("[Env] SMP_MASTER_KEY contains non-hex characters");
        }
    }

    // --- Optional vars ---
    c.port          = optionalVarU16("SMP_PORT",      443);
    c.logLevel      = optionalVar("SMP_LOG_LEVEL",    "INFO");
    c.tlsCertPath   = optionalVar("SMP_TLS_CERT_PATH","");
    c.tlsKeyPath    = optionalVar("SMP_TLS_KEY_PATH", "");
    c.dbPoolSize    = optionalVarU32("SMP_DB_POOL_SIZE", 20);
    c.workerThreads = optionalVarU32("SMP_WORKER_THREADS",
                                     static_cast<uint32_t>(std::thread::hardware_concurrency()));

    // Session secret: use explicit var or fall back to master key
    // (different semantic purpose - allows independent rotation)
    auto sessionSecretOpt = optionalVar("SMP_SESSION_SECRET");
    c.sessionSecret = sessionSecretOpt.value_or(c.masterKey);

    // --- Validate log level ---
    const std::vector<std::string_view> validLevels = {
        "TRACE", "DEBUG", "INFO", "WARN", "ERROR"
    };
    const bool validLevel = std::any_of(validLevels.begin(), validLevels.end(),
        [&](std::string_view lv) { return lv == c.logLevel; });
    if (!validLevel) {
        throw std::runtime_error(
            "[Env] SMP_LOG_LEVEL must be one of: TRACE, DEBUG, INFO, WARN, ERROR"
        );
    }

    // --- Warn on TLS misconfiguration (one path set, not the other) ---
    if (c.tlsCertPath.empty() != c.tlsKeyPath.empty()) {
        throw std::runtime_error(
            "[Env] SMP_TLS_CERT_PATH and SMP_TLS_KEY_PATH must both be set or both be empty"
        );
    }
    if (c.tlsCertPath.empty()) {
        std::cerr << "[Env] WARNING: TLS not configured. "
                     "SMP spec requires HTTPS on port 443. "
                     "Only acceptable in development.\n";
    }

    c.validated = true;

    // --- Log resolved config (no secrets) ---
    std::cout << "[Env] Configuration validated:\n"
              << "  SMP_PORT:           " << c.port          << "\n"
              << "  SMP_LOG_LEVEL:      " << c.logLevel      << "\n"
              << "  SMP_DB_POOL_SIZE:   " << c.dbPoolSize    << "\n"
              << "  SMP_WORKER_THREADS: " << c.workerThreads << "\n"
              << "  TLS enabled:        " << (c.tlsCertPath.empty() ? "no" : "yes") << "\n"
              << "  SMP_MASTER_KEY:     [set, not logged]\n"
              << "  SMP_ADMIN_PASSWORD: [set, not logged]\n";
}

// -----------------------------------------------------------------------------
// Accessors - all assert validated state first
// -----------------------------------------------------------------------------
static void assertValidated(const Environment::Cache& c) {
    if (!c.validated) [[unlikely]] {
        throw std::logic_error("[Env] Environment::validate() must be called before accessing config");
    }
}

const std::string& Environment::masterKey() {
    const auto& c = cache();
    assertValidated(c);
    return c.masterKey;
}

const std::string& Environment::dbUrl() {
    const auto& c = cache();
    assertValidated(c);
    return c.dbUrl;
}

const std::string& Environment::adminPassword() {
    const auto& c = cache();
    assertValidated(c);
    return c.adminPassword;
}

uint16_t Environment::port() {
    const auto& c = cache();
    assertValidated(c);
    return c.port;
}

const std::string& Environment::logLevel() {
    const auto& c = cache();
    assertValidated(c);
    return c.logLevel;
}

const std::string& Environment::sessionSecret() {
    const auto& c = cache();
    assertValidated(c);
    return c.sessionSecret;
}

const std::string& Environment::tlsCertPath() {
    const auto& c = cache();
    assertValidated(c);
    return c.tlsCertPath;
}

const std::string& Environment::tlsKeyPath() {
    const auto& c = cache();
    assertValidated(c);
    return c.tlsKeyPath;
}

uint32_t Environment::workerThreads() {
    const auto& c = cache();
    assertValidated(c);
    return c.workerThreads;
}

uint32_t Environment::dbPoolSize() {
    const auto& c = cache();
    assertValidated(c);
    return c.dbPoolSize;
}

std::vector<uint8_t> Environment::masterKeyBytes() {
    return hexDecode(masterKey());
}

bool Environment::tlsEnabled() {
    return !tlsCertPath().empty();
}

} // namespace smp