//
// Created by Saadat Baig on 28.06.26.
//
#include "user_repository.h"

#include <libpq-fe.h>
#include <stdexcept>

namespace SMPPlusPlus {

// -----------------------------------------------------------------------------
// Async - findByUsername via Drogon coroutine DB client
// -----------------------------------------------------------------------------
drogon::Task<std::optional<User>> UserRepository::findByUsername(
    const std::string& username)
{
    auto db = drogon::app().getFastDbClient("default");

    try {
        auto result = co_await db->execSqlCoro(
            "SELECT id, username, password_hash, role, is_active "
            "FROM users WHERE username = $1 AND is_active = true LIMIT 1",
            username
        );

        if (result.empty()) {
            co_return std::nullopt;
        }

        const auto& row = result[0];
        User user;
        user.id           = row["id"].as<std::string>();
        user.username     = row["username"].as<std::string>();
        user.passwordHash = row["password_hash"].as<std::string>();
        user.role         = row["role"].as<std::string>();
        user.isActive     = row["is_active"].as<bool>();
        co_return user;

    } catch (const drogon::orm::DrogonDbException& e) {
        LOG_ERROR << "[UserRepository] findByUsername failed: " << e.base().what();
        co_return std::nullopt;
    }
}

// -----------------------------------------------------------------------------
// Sync via libpq - for startup use only
// -----------------------------------------------------------------------------
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