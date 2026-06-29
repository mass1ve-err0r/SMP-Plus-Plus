//
// Created by Saadat Baig on 28.06.26.
//
#pragma once

#include <string>
#include <vector>
#include <optional>
#include <drogon/drogon.h>


namespace SMPPlusPlus
{
    struct User {
        std::string id;
        std::string username;
        std::string passwordHash;
        std::string role;
        bool        isActive;
        std::string createdAt;
        std::string createdBy;
    };

    struct TenantGrant {
        std::string userId;
        std::string tenantId;
        std::string tenantName;
        std::string tenantSlug;
        std::string grantedBy;
        std::string grantedAt;
    };

    class UserRepository {
    public:
        UserRepository() = delete;

        // -------------------------------------------------------------------------
        // Queries
        // -------------------------------------------------------------------------
        static drogon::Task<std::vector<User>> findAll();

        static drogon::Task<std::optional<User>> findById(
            const std::string& id
        );

        static drogon::Task<std::optional<User>> findByUsername(
            const std::string& username
        );

        static drogon::Task<bool> usernameExists(
            const std::string& username
        );

        // -------------------------------------------------------------------------
        // Mutations
        // -------------------------------------------------------------------------
        static drogon::Task<std::string> create(
            const std::string& username,
            const std::string& passwordHash,
            const std::string& role,
            const std::string& createdByUserId
        );

        static drogon::Task<bool> updatePassword(
            const std::string& id,
            const std::string& newPasswordHash
        );

        static drogon::Task<bool> deactivate(
            const std::string& id
        );

        static drogon::Task<bool> reactivate(
            const std::string& id
        );

        static drogon::Task<bool> hardDelete(
            const std::string& id
        );

        // -------------------------------------------------------------------------
        // Tenant grants
        // -------------------------------------------------------------------------
        static drogon::Task<std::vector<TenantGrant>> findGrantsByUserId(
            const std::string& userId
        );

        static drogon::Task<bool> grantExists(
            const std::string& userId,
            const std::string& tenantId
        );

        static drogon::Task<bool> addGrant(
            const std::string& userId,
            const std::string& tenantId,
            const std::string& grantedByUserId
        );

        static drogon::Task<bool> removeGrant(
            const std::string& userId,
            const std::string& tenantId
        );

        // -------------------------------------------------------------------------
        // Sync via libpq - startup use only (before app().run())
        // -------------------------------------------------------------------------
        static bool existsByUsername(
            const std::string& connString,
            const std::string& username
        );

        static void createUser(
            const std::string& connString,
            const std::string& username,
            const std::string& passwordHash,
            const std::string& role
        );
    };

} // namespace SMPPlusPlus
