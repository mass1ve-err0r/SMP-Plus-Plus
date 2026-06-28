//
// Created by Saadat Baig on 28.06.26.
//
#include "migration_runner.h"

#include "migrations/migration_001.h"

#include <libpq-fe.h>
#include <iostream>
#include <stdexcept>
#include <string>


namespace SMPPlusPlus
{
    // -----------------------------------------------------------------------------
    // Migration registry - add new entries here as migrations are added
    // -----------------------------------------------------------------------------
    const std::vector<MigrationRunner::Migration>& MigrationRunner::migrations() {
        static const std::vector<Migration> all = {
            { "001", Migrations::migration_001_sql },
            // { "002", Migrations::migration_002_sql },
        };
        return all;
    }

    // -----------------------------------------------------------------------------
    // run() - opens a direct libpq connection, applies pending migrations, closes
    // -----------------------------------------------------------------------------
    void MigrationRunner::run(const std::string& connString) {
        std::cout << "[MigrationRunner] Connecting...\n";

        PGconn* conn = PQconnectdb(connString.c_str());
        if (PQstatus(conn) != CONNECTION_OK) {
            std::string err = PQerrorMessage(conn);
            PQfinish(conn);
            throw std::runtime_error(
                "[MigrationRunner] Failed to connect to database: " + err
            );
        }

        std::cout << "[MigrationRunner] Connected. Checking migrations...\n";

        try {
            for (const auto& m : migrations()) {
                if (isApplied(conn, m.version)) {
                    std::cout << "[MigrationRunner] " << m.version
                              << " already applied, skipping.\n";
                    continue;
                }
                applyMigration(conn, m);
            }
        } catch (...) {
            PQfinish(conn);
            throw;
        }

        PQfinish(conn);
        std::cout << "[MigrationRunner] All migrations up to date.\n";
    }

    // -----------------------------------------------------------------------------
    // execOrThrow - execute SQL and throw on failure
    // -----------------------------------------------------------------------------
    void MigrationRunner::execOrThrow(PGconn* conn, const std::string& sql, std::string_view context) {
        PGresult* res = PQexec(conn, sql.c_str());
        ExecStatusType status = PQresultStatus(res);

        if (status != PGRES_COMMAND_OK && status != PGRES_TUPLES_OK) {
            std::string err = PQresultErrorMessage(res);
            PQclear(res);
            throw std::runtime_error(
                std::string("[MigrationRunner] ") + std::string(context) + ": " + err
            );
        }
        PQclear(res);
    }

    // -----------------------------------------------------------------------------
    // isApplied - check schema_migrations table
    // Returns false gracefully if the table doesn't exist yet (first run)
    // -----------------------------------------------------------------------------
    bool MigrationRunner::isApplied(PGconn* conn, std::string_view version) {
        // Check if schema_migrations exists first
        const std::string checkTable =
            "SELECT 1 FROM information_schema.tables "
            "WHERE table_schema = 'public' AND table_name = 'schema_migrations'";

        PGresult* res = PQexec(conn, checkTable.c_str());
        bool tableExists = (PQntuples(res) > 0);
        PQclear(res);

        if (!tableExists) return false;

        // Table exists - check for this version
        const std::string versionStr(version);
        const char* params[1] = { versionStr.c_str() };
        res = PQexecParams(conn,
            "SELECT 1 FROM schema_migrations WHERE version = $1 LIMIT 1",
            1, nullptr, params, nullptr, nullptr, 0
        );

        bool applied = (PQntuples(res) > 0);
        PQclear(res);
        return applied;
    }

    // -----------------------------------------------------------------------------
    // applyMigration - run in a transaction, record on success
    // -----------------------------------------------------------------------------
    void MigrationRunner::applyMigration(PGconn* conn, const Migration& m) {
        std::cout << "[MigrationRunner] Applying migration " << m.version << "...\n";

        execOrThrow(conn, "BEGIN", "BEGIN transaction");

        try {
            execOrThrow(conn, std::string(m.sql), m.version);

            const std::string versionStr(m.version);
            const char* params[1] = { versionStr.c_str() };
            PGresult* res = PQexecParams(conn,
                "INSERT INTO schema_migrations (version) VALUES ($1)",
                1, nullptr, params, nullptr, nullptr, 0
            );
            if (PQresultStatus(res) != PGRES_COMMAND_OK) {
                std::string err = PQresultErrorMessage(res);
                PQclear(res);
                throw std::runtime_error(
                    "[MigrationRunner] Failed to record migration " +
                    std::string(m.version) + ": " + err
                );
            }
            PQclear(res);

            execOrThrow(conn, "COMMIT", "COMMIT transaction");

            std::cout << "[MigrationRunner] Migration " << m.version
                      << " applied successfully.\n";

        } catch (...) {
            PQexec(conn, "ROLLBACK");
            throw;
        }
    }

} // namespace SMPPlusPlus
