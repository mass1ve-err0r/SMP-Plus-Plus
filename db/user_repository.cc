//
// Created by Saadat Baig on 28.06.26.
//
#include "user_repository.h"

#include <libpq-fe.h>
#include <stdexcept>


namespace SMPPlusPlus
{
    namespace {

        User rowToUser(const drogon::orm::Row& row) {
            User u;
            u.id           = row["id"].as<std::string>();
            u.username     = row["username"].as<std::string>();
            u.passwordHash = row["password_hash"].as<std::string>();
            u.role         = row["role"].as<std::string>();
            u.isActive     = row["is_active"].as<bool>();
            u.createdAt    = row["created_at"].as<std::string>();
            if (!row["created_by"].isNull())
                u.createdBy = row["created_by"].as<std::string>();
            return u;
        }

    } // anonymous namespace

    // =============================================================================
    // Queries
    // =============================================================================

    drogon::Task<std::vector<User>> UserRepository::findAll() {
        auto db = drogon::app().getFastDbClient("default");
        std::vector<User> results;

        try {
            auto rows = co_await db->execSqlCoro(
                "SELECT id, username, password_hash, role, is_active, "
                "created_at, created_by FROM users ORDER BY username ASC"
            );
            for (const auto& row : rows)
                results.push_back(rowToUser(row));
        } catch (const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "[UserRepository] findAll: " << e.base().what();
        }
        co_return results;
    }

    drogon::Task<std::optional<User>> UserRepository::findById(
        const std::string& id)
    {
        auto db = drogon::app().getFastDbClient("default");

        try {
            auto rows = co_await db->execSqlCoro(
                "SELECT id, username, password_hash, role, is_active, "
                "created_at, created_by FROM users WHERE id = $1 LIMIT 1",
                id
            );
            if (!rows.empty()) co_return rowToUser(rows[0]);
        } catch (const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "[UserRepository] findById: " << e.base().what();
        }
        co_return std::nullopt;
    }

    drogon::Task<std::optional<User>> UserRepository::findByUsername(
        const std::string& username)
    {
        auto db = drogon::app().getFastDbClient("default");

        try {
            auto rows = co_await db->execSqlCoro(
                "SELECT id, username, password_hash, role, is_active, "
                "created_at, created_by FROM users "
                "WHERE username = $1 AND is_active = true LIMIT 1",
                username
            );
            if (!rows.empty()) co_return rowToUser(rows[0]);
        } catch (const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "[UserRepository] findByUsername: " << e.base().what();
        }
        co_return std::nullopt;
    }

    drogon::Task<bool> UserRepository::usernameExists(
        const std::string& username)
    {
        auto db = drogon::app().getFastDbClient("default");

        try {
            auto rows = co_await db->execSqlCoro(
                "SELECT 1 FROM users WHERE username = $1 LIMIT 1",
                username
            );
            co_return !rows.empty();
        } catch (const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "[UserRepository] usernameExists: " << e.base().what();
            co_return false;
        }
    }

    // =============================================================================
    // Mutations
    // =============================================================================

    drogon::Task<std::string> UserRepository::create(
        const std::string& username,
        const std::string& passwordHash,
        const std::string& role,
        const std::string& createdByUserId)
    {
        auto db = drogon::app().getFastDbClient("default");

        try {
            auto rows = co_await db->execSqlCoro(
                "INSERT INTO users (username, password_hash, role, created_by) "
                "VALUES ($1, $2, $3, $4) RETURNING id",
                username, passwordHash, role, createdByUserId
            );
            if (!rows.empty()) co_return rows[0]["id"].as<std::string>();
        } catch (const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "[UserRepository] create: " << e.base().what();
            throw;
        }
        co_return std::string{};
    }

    drogon::Task<bool> UserRepository::updatePassword(
        const std::string& id,
        const std::string& newPasswordHash)
    {
        auto db = drogon::app().getFastDbClient("default");

        try {
            auto result = co_await db->execSqlCoro(
                "UPDATE users SET password_hash = $1 WHERE id = $2",
                newPasswordHash, id
            );
            co_return result.affectedRows() > 0;
        } catch (const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "[UserRepository] updatePassword: " << e.base().what();
            co_return false;
        }
    }

    drogon::Task<bool> UserRepository::deactivate(const std::string& id) {
        auto db = drogon::app().getFastDbClient("default");

        try {
            auto result = co_await db->execSqlCoro(
                "UPDATE users SET is_active = false WHERE id = $1 "
                "AND role != 'CORE_ADMIN'",  // core admin can never be deactivated
                id
            );
            co_return result.affectedRows() > 0;
        } catch (const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "[UserRepository] deactivate: " << e.base().what();
            co_return false;
        }
    }

    drogon::Task<bool> UserRepository::reactivate(const std::string& id) {
        auto db = drogon::app().getFastDbClient("default");

        try {
            auto result = co_await db->execSqlCoro(
                "UPDATE users SET is_active = true WHERE id = $1 "
                "AND role != 'CORE_ADMIN'",
                id
            );
            co_return result.affectedRows() > 0;
        } catch (const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "[UserRepository] reactivate: " << e.base().what();
            co_return false;
        }
    }

    drogon::Task<bool> UserRepository::hardDelete(const std::string& id) {
        auto db = drogon::app().getFastDbClient("default");

        try {
            auto result = co_await db->execSqlCoro(
                "DELETE FROM users WHERE id = $1 AND role != 'CORE_ADMIN'",
                id
            );
            co_return result.affectedRows() > 0;
        } catch (const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "[UserRepository] hardDelete: " << e.base().what();
            co_return false;
        }
    }

    // =============================================================================
    // Tenant grants
    // =============================================================================

    drogon::Task<std::vector<TenantGrant>> UserRepository::findGrantsByUserId(
        const std::string& userId)
    {
        auto db = drogon::app().getFastDbClient("default");
        std::vector<TenantGrant> results;

        try {
            auto rows = co_await db->execSqlCoro(
                "SELECT g.user_id, g.tenant_id, t.name AS tenant_name, "
                "t.slug AS tenant_slug, g.granted_by, g.granted_at "
                "FROM user_tenant_grants g "
                "INNER JOIN tenants t ON t.id = g.tenant_id "
                "WHERE g.user_id = $1 ORDER BY t.name ASC",
                userId
            );
            for (const auto& row : rows) {
                TenantGrant g;
                g.userId     = row["user_id"].as<std::string>();
                g.tenantId   = row["tenant_id"].as<std::string>();
                g.tenantName = row["tenant_name"].as<std::string>();
                g.tenantSlug = row["tenant_slug"].as<std::string>();
                g.grantedAt  = row["granted_at"].as<std::string>();
                if (!row["granted_by"].isNull())
                    g.grantedBy = row["granted_by"].as<std::string>();
                results.push_back(g);
            }
        } catch (const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "[UserRepository] findGrantsByUserId: " << e.base().what();
        }
        co_return results;
    }

    drogon::Task<bool> UserRepository::grantExists(
        const std::string& userId,
        const std::string& tenantId)
    {
        auto db = drogon::app().getFastDbClient("default");

        try {
            auto rows = co_await db->execSqlCoro(
                "SELECT 1 FROM user_tenant_grants "
                "WHERE user_id = $1 AND tenant_id = $2 LIMIT 1",
                userId, tenantId
            );
            co_return !rows.empty();
        } catch (const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "[UserRepository] grantExists: " << e.base().what();
            co_return false;
        }
    }

    drogon::Task<bool> UserRepository::addGrant(
        const std::string& userId,
        const std::string& tenantId,
        const std::string& grantedByUserId)
    {
        auto db = drogon::app().getFastDbClient("default");

        try {
            auto result = co_await db->execSqlCoro(
                "INSERT INTO user_tenant_grants (user_id, tenant_id, granted_by) "
                "VALUES ($1, $2, $3) ON CONFLICT DO NOTHING",
                userId, tenantId, grantedByUserId
            );
            co_return true;
        } catch (const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "[UserRepository] addGrant: " << e.base().what();
            co_return false;
        }
    }

    drogon::Task<bool> UserRepository::removeGrant(
        const std::string& userId,
        const std::string& tenantId)
    {
        auto db = drogon::app().getFastDbClient("default");

        try {
            auto result = co_await db->execSqlCoro(
                "DELETE FROM user_tenant_grants "
                "WHERE user_id = $1 AND tenant_id = $2",
                userId, tenantId
            );
            co_return result.affectedRows() > 0;
        } catch (const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "[UserRepository] removeGrant: " << e.base().what();
            co_return false;
        }
    }

    // =============================================================================
    // Sync via libpq - startup use only
    // =============================================================================

    bool UserRepository::existsByUsername(
        const std::string& connString,
        const std::string& username)
    {
        PGconn* conn = PQconnectdb(connString.c_str());
        if (PQstatus(conn) != CONNECTION_OK) {
            std::string err = PQerrorMessage(conn);
            PQfinish(conn);
            throw std::runtime_error("[UserRepository] DB connection failed: " + err);
        }

        const char* params[1] = { username.c_str() };
        PGresult* res = PQexecParams(conn,
            "SELECT 1 FROM users WHERE username = $1 LIMIT 1",
            1, nullptr, params, nullptr, nullptr, 0
        );

        bool exists = (PQntuples(res) > 0);
        PQclear(res);
        PQfinish(conn);
        return exists;
    }

    void UserRepository::createUser(
        const std::string& connString,
        const std::string& username,
        const std::string& passwordHash,
        const std::string& role)
    {
        PGconn* conn = PQconnectdb(connString.c_str());
        if (PQstatus(conn) != CONNECTION_OK) {
            std::string err = PQerrorMessage(conn);
            PQfinish(conn);
            throw std::runtime_error("[UserRepository] DB connection failed: " + err);
        }

        const char* params[3] = {
            username.c_str(),
            passwordHash.c_str(),
            role.c_str()
        };

        PGresult* res = PQexecParams(conn,
            "INSERT INTO users (username, password_hash, role) VALUES ($1, $2, $3)",
            3, nullptr, params, nullptr, nullptr, 0
        );

        if (PQresultStatus(res) != PGRES_COMMAND_OK) {
            std::string err = PQresultErrorMessage(res);
            PQclear(res);
            PQfinish(conn);
            throw std::runtime_error("[UserRepository] createUser failed: " + err);
        }

        PQclear(res);
        PQfinish(conn);
    }

} // namespace SMPPlusPlus
