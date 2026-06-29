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
    struct Tenant {
        std::string id;
        std::string name;
        std::string slug;
        std::string createdAt;
        std::string createdBy; // user id, may be empty
    };

    class TenantRepository {
    public:
        TenantRepository() = delete;

        // Returns all tenants - CORE_ADMIN only
        static drogon::Task<std::vector<Tenant>> findAll();

        // Returns tenants the given user has been granted access to
        static drogon::Task<std::vector<Tenant>> findByUserId(
            const std::string& userId
        );

        static drogon::Task<std::optional<Tenant>> findById(
            const std::string& id
        );

        static drogon::Task<std::optional<Tenant>> findBySlug(
            const std::string& slug
        );

        // Returns the created tenant's id
        static drogon::Task<std::string> create(
            const std::string& name,
            const std::string& slug,
            const std::string& createdByUserId
        );

        static drogon::Task<bool> update(
            const std::string& id,
            const std::string& name,
            const std::string& slug
        );

        static drogon::Task<bool> remove(
            const std::string& id
        );

        static drogon::Task<bool> slugExists(
            const std::string& slug
        );
    };

} // namespace SMPPlusPlus
