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
    struct SmlConfiguration {
        std::string id;
        std::string label;
        std::string dnsZone;
        std::string managementEndpoint;
        std::string smpId;
        bool        isPrimary;
        bool        isEnabled;
        bool        autoRegister;
        std::string createdAt;
    };

    class SmlRepository {
    public:
        SmlRepository() = delete;

        static drogon::Task<std::vector<SmlConfiguration>> findAll();

        static drogon::Task<std::optional<SmlConfiguration>> findById(
            const std::string& id
        );

        static drogon::Task<std::optional<SmlConfiguration>> findPrimary();

        static drogon::Task<std::string> create(
            const std::string& label,
            const std::string& dnsZone,
            const std::string& managementEndpoint,
            const std::string& smpId,
            bool isPrimary,
            bool isEnabled,
            bool autoRegister
        );

        static drogon::Task<bool> update(
            const std::string& id,
            const std::string& label,
            const std::string& dnsZone,
            const std::string& managementEndpoint,
            const std::string& smpId,
            bool autoRegister
        );

        static drogon::Task<bool> remove(const std::string& id);

        static drogon::Task<bool> enable(const std::string& id);
        static drogon::Task<bool> disable(const std::string& id);

        // Atomically clears primary from all, sets on target
        static drogon::Task<bool> setPrimary(const std::string& id);
    };

} // namespace SMPPlusPlus
