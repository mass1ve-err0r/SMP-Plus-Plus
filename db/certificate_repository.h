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
    struct SmpCertificate {
        std::string id;
        std::string label;
        std::string certPem;
        std::string privateKeyEnc;  // AES-256-GCM blob - never sent to client
        std::string fingerprint;
        std::string validFrom;
        std::string validUntil;
        bool        isActive;
        std::string createdAt;
        std::string createdBy;
    };

    class CertificateRepository {
    public:
        CertificateRepository() = delete;

        static drogon::Task<std::vector<SmpCertificate>> findAll();

        static drogon::Task<std::optional<SmpCertificate>> findById(
            const std::string& id
        );

        static drogon::Task<std::optional<SmpCertificate>> findActive();

        static drogon::Task<std::string> create(
            const std::string& label,
            const std::string& certPem,
            const std::string& privateKeyEnc,
            const std::string& fingerprint,
            const std::string& validFrom,
            const std::string& validUntil,
            const std::string& createdByUserId
        );

        // Deactivates all certs then activates the given one atomically
        static drogon::Task<bool> activate(const std::string& id);

        static drogon::Task<bool> remove(const std::string& id);
    };

} // namespace SMPPlusPlus
