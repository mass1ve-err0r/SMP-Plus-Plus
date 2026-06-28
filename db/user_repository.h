//
// Created by Saadat Baig on 28.06.26.
//
#pragma once

#include <string>
#include <optional>
#include <drogon/drogon.h>


namespace SMPPlusPlus {

    /**
     * UserRepository - DB access for the users table.
     *
     * All async methods use co_await and must be called from within
     * a Drogon coroutine (after app().run()).
     *
     * findByUsername() and exists() are also provided as synchronous
     * libpq variants for use during startup (admin bootstrap).
     */
    struct User {
        std::string id;
        std::string username;
        std::string passwordHash;
        std::string role;       // "CORE_ADMIN" | "MANAGEMENT"
        bool        isActive;
    };

    class UserRepository {
    public:
        UserRepository() = delete;

        // -------------------------------------------------------------------------
        // Async - for use inside Drogon coroutines (after app().run())
        // -------------------------------------------------------------------------
        static drogon::Task<std::optional<User>> findByUsername(
            const std::string& username
        );

        // -------------------------------------------------------------------------
        // Sync via libpq - for use during startup before app().run()
        // -------------------------------------------------------------------------
        static bool existsByUsername(
            const std::string& connString,
            const std::string& username
        );

        static void createUser(
            const std::string& connString,
            const std::string& username,
            const std::string& passwordHash,
            const std::string& role         // "CORE_ADMIN" | "MANAGEMENT"
        );
    };

} // namespace SMPPlusPlus