//
// Created by Saadat Baig on 07.07.26.
//
#include "session_repository.h"
#include "utils/crypto_utils.h"


namespace SMPPlusPlus
{
    namespace {

        Session rowToSession(const drogon::orm::Row& row) {
            Session s;
            s.id         = row["id"].as<std::string>();
            s.token      = row["token"].as<std::string>();
            s.userId     = row["user_id"].as<std::string>();
            s.createdAt  = row["created_at"].as<std::string>();
            s.expiresAt  = row["expires_at"].as<std::string>();
            s.lastUsedAt = row["last_used_at"].as<std::string>();
            if (!row["user_agent"].isNull())
                s.userAgent = row["user_agent"].as<std::string>();
            if (!row["ip_address"].isNull())
                s.ipAddress = row["ip_address"].as<std::string>();
            return s;
        }

        // Base64url without padding - URL-safe, cookie-safe
        std::string base64UrlNoPad(const std::vector<uint8_t>& bytes) {
            std::string s = CryptoUtils::base64Encode(
                std::span<const uint8_t>(bytes.data(), bytes.size())
            );
            // Convert standard base64 to base64url and strip padding
            for (auto& c : s) {
                if (c == '+') c = '-';
                else if (c == '/') c = '_';
            }
            while (!s.empty() && s.back() == '=') s.pop_back();
            return s;
        }

    } // anonymous namespace

    drogon::Task<std::string> SessionRepository::create(
        const std::string& userId,
        const std::string& userAgent,
        const std::string& ipAddress,
        int                slidingWindowHours)
    {
        auto db = drogon::app().getFastDbClient("default");

        auto randomBytes = CryptoUtils::randomBytes(32);
        if (!randomBytes) {
            LOG_ERROR << "[SessionRepository] create: randomBytes failed";
            co_return std::string{};
        }
        const std::string token = base64UrlNoPad(*randomBytes);

        try {
            co_await db->execSqlCoro(
                "INSERT INTO sessions "
                "(token, user_id, expires_at, user_agent, ip_address) "
                "VALUES ($1, $2, now() + ($3 || ' hours')::INTERVAL, "
                "NULLIF($4, ''), NULLIF($5, ''))",
                token, userId, std::to_string(slidingWindowHours),
                userAgent, ipAddress
            );
            co_return token;
        } catch (const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "[SessionRepository] create: " << e.base().what();
            co_return std::string{};
        }
    }

    drogon::Task<std::optional<Session>> SessionRepository::findByToken(
        const std::string& token)
    {
        auto db = drogon::app().getFastDbClient("default");

        try {
            auto rows = co_await db->execSqlCoro(
                "SELECT id, token, user_id, created_at, expires_at, "
                "last_used_at, user_agent, ip_address FROM sessions "
                "WHERE token = $1 AND expires_at > now() LIMIT 1",
                token
            );
            if (!rows.empty()) co_return rowToSession(rows[0]);
        } catch (const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "[SessionRepository] findByToken: " << e.base().what();
        }
        co_return std::nullopt;
    }

    drogon::Task<std::string> SessionRepository::extend(
        const std::string& token,
        int                slidingWindowHours,
        int                maxLifetimeHours)
    {
        auto db = drogon::app().getFastDbClient("default");

        try {
            // Set expires_at = LEAST(now() + slidingWindow, created_at + maxLifetime)
            auto rows = co_await db->execSqlCoro(
                "UPDATE sessions SET "
                "  expires_at = LEAST("
                "    now() + ($2 || ' hours')::INTERVAL, "
                "    created_at + ($3 || ' hours')::INTERVAL"
                "  ), "
                "  last_used_at = now() "
                "WHERE token = $1 AND expires_at > now() "
                "RETURNING expires_at",
                token,
                std::to_string(slidingWindowHours),
                std::to_string(maxLifetimeHours)
            );
            if (!rows.empty()) co_return rows[0]["expires_at"].as<std::string>();
        } catch (const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "[SessionRepository] extend: " << e.base().what();
        }
        co_return std::string{};
    }

    drogon::Task<bool> SessionRepository::revokeByToken(const std::string& token) {
        auto db = drogon::app().getFastDbClient("default");

        try {
            auto result = co_await db->execSqlCoro(
                "DELETE FROM sessions WHERE token = $1",
                token
            );
            co_return result.affectedRows() > 0;
        } catch (const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "[SessionRepository] revokeByToken: " << e.base().what();
            co_return false;
        }
    }

    drogon::Task<bool> SessionRepository::revokeByIdForUser(
        const std::string& sessionId,
        const std::string& userId)
    {
        auto db = drogon::app().getFastDbClient("default");

        try {
            auto result = co_await db->execSqlCoro(
                "DELETE FROM sessions WHERE id = $1 AND user_id = $2",
                sessionId, userId
            );
            co_return result.affectedRows() > 0;
        } catch (const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "[SessionRepository] revokeByIdForUser: " << e.base().what();
            co_return false;
        }
    }

    drogon::Task<std::vector<Session>> SessionRepository::findByUserId(
        const std::string& userId)
    {
        auto db = drogon::app().getFastDbClient("default");
        std::vector<Session> results;

        try {
            auto rows = co_await db->execSqlCoro(
                "SELECT id, token, user_id, created_at, expires_at, "
                "last_used_at, user_agent, ip_address FROM sessions "
                "WHERE user_id = $1 AND expires_at > now() "
                "ORDER BY last_used_at DESC",
                userId
            );
            for (const auto& row : rows)
                results.push_back(rowToSession(row));
        } catch (const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "[SessionRepository] findByUserId: " << e.base().what();
        }
        co_return results;
    }

    drogon::Task<bool> SessionRepository::revokeAllForUser(const std::string& userId) {
        auto db = drogon::app().getFastDbClient("default");

        try {
            co_await db->execSqlCoro(
                "DELETE FROM sessions WHERE user_id = $1",
                userId
            );
            co_return true;
        } catch (const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "[SessionRepository] revokeAllForUser: " << e.base().what();
            co_return false;
        }
    }

    drogon::Task<int> SessionRepository::deleteExpired() {
        auto db = drogon::app().getFastDbClient("default");

        try {
            auto result = co_await db->execSqlCoro(
                "DELETE FROM sessions WHERE expires_at <= now()"
            );
            co_return static_cast<int>(result.affectedRows());
        } catch (const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "[SessionRepository] deleteExpired: " << e.base().what();
            co_return 0;
        }
    }

} // namespace SMPPlusPlus
