//
// Created by Saadat Baig on 28.06.26.
//
#include <drogon/drogon.h>
#include "utils/environment.h"
#include "db/migration_runner.h"
#include "db/admin_bootstrap.h"
#include "db/signing_key_cache.h"


int main()
{
    // -------------------------------------------------------------------------
    // Step 1: Validate all environment variables.
    // Nothing else runs until this succeeds.
    // -------------------------------------------------------------------------
    SMPPlusPlus::Environment::validate();

    auto& app = drogon::app();

    // -------------------------------------------------------------------------
    // Step 2: Load config.json for non-sensitive settings.
    // DB and TLS come from Environment, not config.json.
    // -------------------------------------------------------------------------
    app.loadConfigFile("config.json");

    // -------------------------------------------------------------------------
    // Step 3: Override log level and thread count from Environment.
    // These take precedence over config.json values.
    // -------------------------------------------------------------------------
    app.setLogLevel([] {
        const auto& level = SMPPlusPlus::Environment::logLevel();
        if (level == "TRACE") return trantor::Logger::kTrace;
        if (level == "DEBUG") return trantor::Logger::kDebug;
        if (level == "WARN")  return trantor::Logger::kWarn;
        if (level == "ERROR") return trantor::Logger::kError;
        return trantor::Logger::kInfo;
    }());

    app.setThreadNum(static_cast<size_t>(SMPPlusPlus::Environment::workerThreads()));

    // -------------------------------------------------------------------------
    // Step 4: Register the runtime DB client (is_fast=true).
    // One connection per IO thread, no locking on the query path.
    // Not usable until app.run() starts the event loop.
    // -------------------------------------------------------------------------
    const auto& db = SMPPlusPlus::Environment::dbConfig();
    app.createDbClient(
        "postgresql",                            // dbType
        db.host,                                 // host
        db.port,                                 // port
        db.dbname,                               // databaseName
        db.user,                                 // userName
        db.password,                             // password
        static_cast<size_t>(db.connections),     // connectionNum (per IO thread)
        "",                                      // filename (sqlite3 only)
        "default",                               // name
        true,                                    // isFast
        "utf8",                                  // characterSet
        db.timeout                               // timeout
    );

    // -------------------------------------------------------------------------
    // Step 5: Listener.
    //
    // Three deployment scenarios:
    //   1. SMP_DEV_MODE=true       → plain HTTP, no TLS (local dev only).
    //   2. SMP_BEHIND_PROXY=true   → plain HTTP internally, nginx/Cloudflare
    //                                terminates TLS upstream (typical production).
    //   3. otherwise               → TLS handled directly by smp++,
    //                                SMP_TLS_CERT_PATH + SMP_TLS_KEY_PATH required.
    // -------------------------------------------------------------------------
    if (SMPPlusPlus::Environment::devMode()) {
        LOG_WARN << "[smp++] DEV MODE: running plain HTTP on port "
                 << SMPPlusPlus::Environment::port()
                 << " - DO NOT use in production";
        app.addListener("0.0.0.0", SMPPlusPlus::Environment::port(), false);
    } else if (SMPPlusPlus::Environment::behindProxy()) {
        LOG_INFO << "[smp++] BEHIND PROXY: running plain HTTP on port "
                 << SMPPlusPlus::Environment::port()
                 << " - TLS is expected to be terminated by nginx/Cloudflare upstream";
        app.addListener("0.0.0.0", SMPPlusPlus::Environment::port(), false);
    } else if (SMPPlusPlus::Environment::tlsEnabled()) {
        app.addListener(
            "0.0.0.0",
            SMPPlusPlus::Environment::port(),
            true,
            SMPPlusPlus::Environment::tlsCertPath(),
            SMPPlusPlus::Environment::tlsKeyPath()
        );
    } else {
        throw std::runtime_error(
            "[smp++] No valid listener configuration. Choose one:\n"
            "  - SMP_DEV_MODE=true                          (local dev)\n"
            "  - SMP_BEHIND_PROXY=true                      (behind nginx/Cloudflare)\n"
            "  - SMP_TLS_CERT_PATH=... SMP_TLS_KEY_PATH=... (standalone TLS)"
        );
    }

    // -------------------------------------------------------------------------
    // Step 6: Run DB migrations via direct libpq before accepting traffic.
    //
    // Drogon's DB client is not available until app().run() starts the event
    // loop. We bypass it entirely and use libpq directly for migrations.
    // -------------------------------------------------------------------------
    SMPPlusPlus::MigrationRunner::run(SMPPlusPlus::Environment::dbUrl());

    // -------------------------------------------------------------------------
    // Step 7: Bootstrap core admin user if this is a first boot.
    // Idempotent - skipped if admin already exists.
    // -------------------------------------------------------------------------
    SMPPlusPlus::AdminBootstrap::run(
        SMPPlusPlus::Environment::dbUrl(),
        "admin",
        SMPPlusPlus::Environment::adminPassword()
    );

    // -------------------------------------------------------------------------
    // Step 8: Eagerly load the active SMP signing key into cache.
    // If no active cert exists, the SMP protocol endpoints will return 503
    // until a cert is uploaded and activated via the management API.
    // -------------------------------------------------------------------------
    SMPPlusPlus::SigningKeyCache::instance().loadFromDb(
        SMPPlusPlus::Environment::dbUrl()
    );

    LOG_INFO << "smp++ starting on port " << SMPPlusPlus::Environment::port();
    app.run();

    return 0;

}
