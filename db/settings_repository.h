//
// Created by Saadat Baig on 29.06.26.
//
#pragma once

#include <string>
#include <vector>
#include <optional>
#include <drogon/drogon.h>


namespace SMPPlusPlus
{
    struct Setting {
        std::string key;
        std::string value;
        std::string updatedAt;
        std::string updatedBy;
    };

    class SettingsRepository {
    public:
        SettingsRepository() = delete;

        static drogon::Task<std::vector<Setting>> findAll();

        static drogon::Task<std::optional<Setting>> findByKey(
            const std::string& key
        );

        // Upsert - inserts or updates the value for the given key
        static drogon::Task<bool> upsert(
            const std::string& key,
            const std::string& value,
            const std::string& updatedByUserId
        );
    };

} // namespace SMPPlusPlus
