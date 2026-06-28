//
// Created by Saadat Baig on 28.06.26.
//
#pragma once

#include <string>


namespace SMPPlusPlus
{
    /**
     * AdminBootstrap - Creates the core admin user on first boot.
     *
     * Called once at startup (after migrations, before app().run()).
     * Uses direct libpq + bcrypt synchronously.
     * Idempotent - does nothing if the admin user already exists.
     */
    class AdminBootstrap {
    public:
        AdminBootstrap() = delete;

        /**
         * @param connString  libpq connection URL (from Environment::dbUrl())
         * @param username    Admin username (hardcoded "admin")
         * @param password    Plaintext password (from Environment::adminPassword())
         */
        static void run(
            const std::string& connString,
            const std::string& username,
            const std::string& password
        );
    };

} // namespace SMPPlusPlus
