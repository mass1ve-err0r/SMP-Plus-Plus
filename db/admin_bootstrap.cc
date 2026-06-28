//
// Created by Saadat Baig on 28.06.26.
//
#include "admin_bootstrap.h"
#include "user_repository.h"
#include "utils/crypto_utils.h"

#include <iostream>
#include <stdexcept>


namespace SMPPlusPlus
{
    void AdminBootstrap::run(
        const std::string& connString,
        const std::string& username,
        const std::string& password)
    {
        std::cout << "[AdminBootstrap] Checking core admin user...\n";

        if (UserRepository::existsByUsername(connString, username)) {
            std::cout << "[AdminBootstrap] Core admin user exists, skipping.\n";
            return;
        }

        std::cout << "[AdminBootstrap] Creating core admin user '" << username << "'...\n";

        // bcrypt is blocking CPU work - acceptable here since we're pre-app.run()
        // on the main thread with no IO loop running yet.
        auto hashResult = CryptoUtils::bcryptHash(password);
        if (!hashResult) {
            throw std::runtime_error(
                "[AdminBootstrap] Failed to hash admin password: " +
                hashResult.error().message
            );
        }

        UserRepository::createUser(connString, username, *hashResult, "CORE_ADMIN");

        std::cout << "[AdminBootstrap] Core admin user created successfully.\n";
    }

} // namespace SMPPlusPlus
