//
// Created by Saadat Baig on 28.06.26.
//
#include "tenant_repository.h"


namespace SMPPlusPlus
{
    namespace {
        Tenant rowToTenant(const drogon::orm::Row& row) {
            Tenant t;
            t.id        = row["id"].as<std::string>();
            t.name      = row["name"].as<std::string>();
            t.slug      = row["slug"].as<std::string>();
            t.createdAt = row["created_at"].as<std::string>();
            if (!row["created_by"].isNull())
                t.createdBy = row["created_by"].as<std::string>();
            return t;
        }
    } // anonymous namespace

    // -----------------------------------------------------------------------------
    // findAll - CORE_ADMIN only
    // -----------------------------------------------------------------------------
    drogon::Task<std::vector<Tenant>> TenantRepository::findAll() {
        auto db = drogon::app().getFastDbClient("default");
        std::vector<Tenant> tenants;

        try {
            auto result = co_await db->execSqlCoro(
                "SELECT id, name, slug, created_at, created_by "
                "FROM tenants ORDER BY name ASC"
            );
            for (const auto& row : result)
                tenants.push_back(rowToTenant(row));
        } catch (const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "[TenantRepository] findAll failed: " << e.base().what();
        }

        co_return tenants;
    }

    // -----------------------------------------------------------------------------
    // findByUserId - for MANAGEMENT users with scoped access
    // -----------------------------------------------------------------------------
    drogon::Task<std::vector<Tenant>> TenantRepository::findByUserId(
        const std::string& userId)
    {
        auto db = drogon::app().getFastDbClient("default");
        std::vector<Tenant> tenants;

        try {
            auto result = co_await db->execSqlCoro(
                "SELECT t.id, t.name, t.slug, t.created_at, t.created_by "
                "FROM tenants t "
                "INNER JOIN user_tenant_grants g ON g.tenant_id = t.id "
                "WHERE g.user_id = $1 "
                "ORDER BY t.name ASC",
                userId
            );
            for (const auto& row : result)
                tenants.push_back(rowToTenant(row));
        } catch (const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "[TenantRepository] findByUserId failed: " << e.base().what();
        }

        co_return tenants;
    }

    // -----------------------------------------------------------------------------
    // findById
    // -----------------------------------------------------------------------------
    drogon::Task<std::optional<Tenant>> TenantRepository::findById(
        const std::string& id)
    {
        auto db = drogon::app().getFastDbClient("default");

        try {
            auto result = co_await db->execSqlCoro(
                "SELECT id, name, slug, created_at, created_by "
                "FROM tenants WHERE id = $1 LIMIT 1",
                id
            );
            if (!result.empty())
                co_return rowToTenant(result[0]);
        } catch (const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "[TenantRepository] findById failed: " << e.base().what();
        }

        co_return std::nullopt;
    }

    // -----------------------------------------------------------------------------
    // findBySlug
    // -----------------------------------------------------------------------------
    drogon::Task<std::optional<Tenant>> TenantRepository::findBySlug(
        const std::string& slug)
    {
        auto db = drogon::app().getFastDbClient("default");

        try {
            auto result = co_await db->execSqlCoro(
                "SELECT id, name, slug, created_at, created_by "
                "FROM tenants WHERE slug = $1 LIMIT 1",
                slug
            );
            if (!result.empty())
                co_return rowToTenant(result[0]);
        } catch (const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "[TenantRepository] findBySlug failed: " << e.base().what();
        }

        co_return std::nullopt;
    }

    // -----------------------------------------------------------------------------
    // create
    // -----------------------------------------------------------------------------
    drogon::Task<std::string> TenantRepository::create(
        const std::string& name,
        const std::string& slug,
        const std::string& createdByUserId)
    {
        auto db = drogon::app().getFastDbClient("default");

        try {
            auto result = co_await db->execSqlCoro(
                "INSERT INTO tenants (name, slug, created_by) "
                "VALUES ($1, $2, $3) RETURNING id",
                name,
                slug,
                createdByUserId
            );
            if (!result.empty())
                co_return result[0]["id"].as<std::string>();
        } catch (const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "[TenantRepository] create failed: " << e.base().what();
            throw;
        }

        co_return std::string{};
    }

    // -----------------------------------------------------------------------------
    // update
    // -----------------------------------------------------------------------------
    drogon::Task<bool> TenantRepository::update(
        const std::string& id,
        const std::string& name,
        const std::string& slug)
    {
        auto db = drogon::app().getFastDbClient("default");

        try {
            auto result = co_await db->execSqlCoro(
                "UPDATE tenants SET name = $1, slug = $2 "
                "WHERE id = $3",
                name,
                slug,
                id
            );
            co_return result.affectedRows() > 0;
        } catch (const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "[TenantRepository] update failed: " << e.base().what();
            co_return false;
        }
    }

    // -----------------------------------------------------------------------------
    // remove
    // -----------------------------------------------------------------------------
    drogon::Task<bool> TenantRepository::remove(const std::string& id) {
        auto db = drogon::app().getFastDbClient("default");

        try {
            auto result = co_await db->execSqlCoro(
                "DELETE FROM tenants WHERE id = $1",
                id
            );
            co_return result.affectedRows() > 0;
        } catch (const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "[TenantRepository] remove failed: " << e.base().what();
            co_return false;
        }
    }

    // -----------------------------------------------------------------------------
    // slugExists
    // -----------------------------------------------------------------------------
    drogon::Task<bool> TenantRepository::slugExists(const std::string& slug) {
        auto db = drogon::app().getFastDbClient("default");

        try {
            auto result = co_await db->execSqlCoro(
                "SELECT 1 FROM tenants WHERE slug = $1 LIMIT 1",
                slug
            );
            co_return !result.empty();
        } catch (const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "[TenantRepository] slugExists failed: " << e.base().what();
            co_return false;
        }
    }

} // namespace SMPPlusPlus
