//
// Created by Saadat Baig on 29.06.26.
//
#include "settings_repository.h"


namespace SMPPlusPlus
{
    namespace {

        Setting rowToSetting(const drogon::orm::Row& row) {
            Setting s;
            s.key       = row["key"].as<std::string>();
            s.value     = row["value"].as<std::string>();
            s.updatedAt = row["updated_at"].as<std::string>();
            if (!row["updated_by"].isNull())
                s.updatedBy = row["updated_by"].as<std::string>();
            return s;
        }

    } // anonymous namespace

    drogon::Task<std::vector<Setting>> SettingsRepository::findAll() {
        auto db = drogon::app().getFastDbClient("default");
        std::vector<Setting> results;

        try {
            auto rows = co_await db->execSqlCoro(
                "SELECT key, value, updated_at, updated_by "
                "FROM system_settings ORDER BY key ASC"
            );
            for (const auto& row : rows)
                results.push_back(rowToSetting(row));
        } catch (const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "[SettingsRepository] findAll: " << e.base().what();
        }
        co_return results;
    }

    drogon::Task<std::optional<Setting>> SettingsRepository::findByKey(
        const std::string& key)
    {
        auto db = drogon::app().getFastDbClient("default");

        try {
            auto rows = co_await db->execSqlCoro(
                "SELECT key, value, updated_at, updated_by "
                "FROM system_settings WHERE key = $1 LIMIT 1",
                key
            );
            if (!rows.empty()) co_return rowToSetting(rows[0]);
        } catch (const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "[SettingsRepository] findByKey: " << e.base().what();
        }
        co_return std::nullopt;
    }

    drogon::Task<bool> SettingsRepository::upsert(
        const std::string& key,
        const std::string& value,
        const std::string& updatedByUserId)
    {
        auto db = drogon::app().getFastDbClient("default");

        try {
            auto result = co_await db->execSqlCoro(
                "INSERT INTO system_settings (key, value, updated_at, updated_by) "
                "VALUES ($1, $2, now(), $3) "
                "ON CONFLICT (key) DO UPDATE SET "
                "value = EXCLUDED.value, "
                "updated_at = now(), "
                "updated_by = EXCLUDED.updated_by",
                key, value, updatedByUserId
            );
            co_return true;
        } catch (const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "[SettingsRepository] upsert: " << e.base().what();
            co_return false;
        }
    }

} // namespace SMPPlusPlus
