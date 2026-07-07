//
// Created by Saadat Baig on 07.07.26.
//
#pragma once

#include <string>
#include <vector>
#include <optional>
#include <drogon/drogon.h>


namespace SMPPlusPlus
{
    struct Session {
        std::string id;
        std::string token;
        std::string userId;
        std::string createdAt;
        std::string expiresAt;
        std::string lastUsedAt;
        std::string userAgent;
        std::string ipAddress;
    };

    class SessionRepository {
    public:
        SessionRepository() = delete;

        // Create a new session. Returns the token (random 32 bytes, base64url).
        // Session expiry is set to now() + slidingWindowHours.
        static drogon::Task<std::string> create(
            const std::string& userId,
            const std::string& userAgent,
            const std::string& ipAddress,
            int                slidingWindowHours = 8
        );

        // Look up an active (non-expired) session by token.
        // Returns nullopt if not found or expired.
        static drogon::Task<std::optional<Session>> findByToken(
            const std::string& token
        );

        // Extend session expiry: expires_at = now() + slidingWindowHours,
        // capped at (created_at + maxLifetimeHours).
        // Also updates last_used_at.
        // Returns the new expires_at, or empty string on failure.
        static drogon::Task<std::string> extend(
            const std::string& token,
            int                slidingWindowHours = 8,
            int                maxLifetimeHours   = 168  // 7 days
        );

        // Delete a session by token (logout).
        static drogon::Task<bool> revokeByToken(const std::string& token);

        // Delete a specific session by id, verifying it belongs to the user.
        static drogon::Task<bool> revokeByIdForUser(
            const std::string& sessionId,
            const std::string& userId
        );

        // List all active sessions for a user.
        static drogon::Task<std::vector<Session>> findByUserId(
            const std::string& userId
        );

        // Delete all sessions for a user (used when password changes).
        static drogon::Task<bool> revokeAllForUser(const std::string& userId);

        // Background cleanup - delete expired sessions.
        static drogon::Task<int> deleteExpired();
    };

} // namespace SMPPlusPlus
