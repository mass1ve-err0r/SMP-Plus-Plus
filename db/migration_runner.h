//
// Created by Saadat Baig on 28.06.26.
//
#pragma once

#include <string>
#include <string_view>
#include <vector>

// Forward declare libpq connection type
struct pg_conn;
typedef struct pg_conn PGconn;


namespace SMPPlusPlus
{
    /**
     * MigrationRunner - Applies SQL migrations at startup via direct libpq.
     *
     * Deliberately bypasses Drogon's DB client entirely because Drogon does not
     * initialize its connection pool until app().run() is called. We need to run
     * migrations before the event loop starts, so we open a direct libpq
     * connection, apply pending migrations, then close it.
     *
     * Call MigrationRunner::run(connString) in main() after Env::validate()
     * but before app().run().
     */
    class MigrationRunner {
    public:
        MigrationRunner() = delete;

        /**
         * Run all pending migrations.
         * @param connString  libpq connection string, e.g. "postgresql://user@host/db"
         * Throws std::runtime_error on connection failure or migration failure.
         */
        static void run(const std::string& connString);

    private:
        struct Migration {
            std::string_view version;
            std::string_view sql;
        };

        static const std::vector<Migration>& migrations();

        static void applyMigration(PGconn* conn, const Migration& m);
        static bool isApplied(PGconn* conn, std::string_view version);
        static void execOrThrow(PGconn* conn, const std::string& sql, std::string_view context);
    };

} // namespace SMPPlusPlus
