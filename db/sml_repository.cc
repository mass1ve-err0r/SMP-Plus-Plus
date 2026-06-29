//
// Created by Saadat Baig on 29.06.26.
//
#include "sml_repository.h"


namespace SMPPlusPlus
{
    namespace {

        SmlConfiguration rowToSml(const drogon::orm::Row& row) {
            SmlConfiguration s;
            s.id                  = row["id"].as<std::string>();
            s.label               = row["label"].as<std::string>();
            s.dnsZone             = row["dns_zone"].as<std::string>();
            s.managementEndpoint  = row["management_endpoint"].as<std::string>();
            s.smpId               = row["smp_id"].as<std::string>();
            s.isPrimary           = row["is_primary"].as<bool>();
            s.isEnabled           = row["is_enabled"].as<bool>();
            s.autoRegister        = row["auto_register"].as<bool>();
            s.createdAt           = row["created_at"].as<std::string>();
            return s;
        }

    } // anonymous namespace

    drogon::Task<std::vector<SmlConfiguration>> SmlRepository::findAll() {
        auto db = drogon::app().getFastDbClient("default");
        std::vector<SmlConfiguration> results;

        try {
            auto rows = co_await db->execSqlCoro(
                "SELECT id, label, dns_zone, management_endpoint, smp_id, "
                "is_primary, is_enabled, auto_register, created_at "
                "FROM sml_configurations ORDER BY is_primary DESC, label ASC"
            );
            for (const auto& row : rows)
                results.push_back(rowToSml(row));
        } catch (const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "[SmlRepository] findAll: " << e.base().what();
        }
        co_return results;
    }

    drogon::Task<std::optional<SmlConfiguration>> SmlRepository::findById(
        const std::string& id)
    {
        auto db = drogon::app().getFastDbClient("default");

        try {
            auto rows = co_await db->execSqlCoro(
                "SELECT id, label, dns_zone, management_endpoint, smp_id, "
                "is_primary, is_enabled, auto_register, created_at "
                "FROM sml_configurations WHERE id = $1 LIMIT 1",
                id
            );
            if (!rows.empty()) co_return rowToSml(rows[0]);
        } catch (const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "[SmlRepository] findById: " << e.base().what();
        }
        co_return std::nullopt;
    }

    drogon::Task<std::optional<SmlConfiguration>> SmlRepository::findPrimary() {
        auto db = drogon::app().getFastDbClient("default");

        try {
            auto rows = co_await db->execSqlCoro(
                "SELECT id, label, dns_zone, management_endpoint, smp_id, "
                "is_primary, is_enabled, auto_register, created_at "
                "FROM sml_configurations WHERE is_primary = true LIMIT 1"
            );
            if (!rows.empty()) co_return rowToSml(rows[0]);
        } catch (const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "[SmlRepository] findPrimary: " << e.base().what();
        }
        co_return std::nullopt;
    }

    drogon::Task<std::string> SmlRepository::create(
        const std::string& label,
        const std::string& dnsZone,
        const std::string& managementEndpoint,
        const std::string& smpId,
        bool isPrimary,
        bool isEnabled,
        bool autoRegister)
    {
        auto db = drogon::app().getFastDbClient("default");

        try {
            // If new entry is primary, clear existing primary first
            if (isPrimary) {
                co_await db->execSqlCoro(
                    "UPDATE sml_configurations SET is_primary = false "
                    "WHERE is_primary = true"
                );
            }

            auto rows = co_await db->execSqlCoro(
                "INSERT INTO sml_configurations "
                "(label, dns_zone, management_endpoint, smp_id, "
                "is_primary, is_enabled, auto_register) "
                "VALUES ($1, $2, $3, $4, $5, $6, $7) RETURNING id",
                label, dnsZone, managementEndpoint, smpId,
                isPrimary, isEnabled, autoRegister
            );
            if (!rows.empty()) co_return rows[0]["id"].as<std::string>();
        } catch (const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "[SmlRepository] create: " << e.base().what();
            throw;
        }
        co_return std::string{};
    }

    drogon::Task<bool> SmlRepository::update(
        const std::string& id,
        const std::string& label,
        const std::string& dnsZone,
        const std::string& managementEndpoint,
        const std::string& smpId,
        bool autoRegister)
    {
        auto db = drogon::app().getFastDbClient("default");

        try {
            auto result = co_await db->execSqlCoro(
                "UPDATE sml_configurations SET "
                "label = $1, dns_zone = $2, management_endpoint = $3, "
                "smp_id = $4, auto_register = $5 "
                "WHERE id = $6",
                label, dnsZone, managementEndpoint, smpId, autoRegister, id
            );
            co_return result.affectedRows() > 0;
        } catch (const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "[SmlRepository] update: " << e.base().what();
            co_return false;
        }
    }

    drogon::Task<bool> SmlRepository::remove(const std::string& id) {
        auto db = drogon::app().getFastDbClient("default");

        try {
            // Cannot delete the primary SML - enforced at app level
            // but also guard here
            auto result = co_await db->execSqlCoro(
                "DELETE FROM sml_configurations "
                "WHERE id = $1 AND is_primary = false",
                id
            );
            co_return result.affectedRows() > 0;
        } catch (const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "[SmlRepository] remove: " << e.base().what();
            co_return false;
        }
    }

    drogon::Task<bool> SmlRepository::enable(const std::string& id) {
        auto db = drogon::app().getFastDbClient("default");

        try {
            auto result = co_await db->execSqlCoro(
                "UPDATE sml_configurations SET is_enabled = true WHERE id = $1",
                id
            );
            co_return result.affectedRows() > 0;
        } catch (const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "[SmlRepository] enable: " << e.base().what();
            co_return false;
        }
    }

    drogon::Task<bool> SmlRepository::disable(const std::string& id) {
        auto db = drogon::app().getFastDbClient("default");

        try {
            auto result = co_await db->execSqlCoro(
                "UPDATE sml_configurations SET is_enabled = false WHERE id = $1",
                id
            );
            co_return result.affectedRows() > 0;
        } catch (const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "[SmlRepository] disable: " << e.base().what();
            co_return false;
        }
    }

    drogon::Task<bool> SmlRepository::setPrimary(const std::string& id) {
        auto db = drogon::app().getFastDbClient("default");

        try {
            co_await db->execSqlCoro(
                "UPDATE sml_configurations SET is_primary = false "
                "WHERE is_primary = true"
            );
            auto result = co_await db->execSqlCoro(
                "UPDATE sml_configurations SET is_primary = true WHERE id = $1",
                id
            );
            co_return result.affectedRows() > 0;
        } catch (const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "[SmlRepository] setPrimary: " << e.base().what();
            co_return false;
        }
    }

} // namespace SMPPlusPlus
