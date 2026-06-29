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
    struct Participant {
        std::string id;
        std::string tenantId;
        std::string identifierScheme;
        std::string identifierValue;
        std::string createdAt;
        std::string createdBy;
    };

    struct Endpoint {
        std::string id;
        std::string processId;
        std::string transportProfile;
        std::string endpointUrl;
        std::string apCertificatePem;
        bool        requireBls;
        std::string serviceActivationDate;
        std::string serviceExpirationDate;
        std::string technicalContactUrl;
        std::string technicalInformationUrl;
        std::string serviceDescription;
        std::string createdAt;
    };

    struct Process {
        std::string id;
        std::string serviceMetadataId;
        std::string processScheme;
        std::string processValue;
        std::string createdAt;
        std::vector<Endpoint> endpoints;
    };

    struct ServiceMetadata {
        std::string id;
        std::string participantId;
        std::string documentTypeScheme;
        std::string documentTypeValue;
        std::string createdAt;
        std::vector<Process> processes;
    };

    class ParticipantRepository {
    public:
        ParticipantRepository() = delete;

        // -------------------------------------------------------------------------
        // Participants
        // -------------------------------------------------------------------------
        static drogon::Task<std::vector<Participant>> findByTenantId(
            const std::string& tenantId
        );

        static drogon::Task<std::optional<Participant>> findById(
            const std::string& id,
            const std::string& tenantId
        );

        static drogon::Task<bool> existsByIdentifier(
            const std::string& tenantId,
            const std::string& scheme,
            const std::string& value
        );

        static drogon::Task<std::string> create(
            const std::string& tenantId,
            const std::string& scheme,
            const std::string& value,
            const std::string& createdByUserId
        );

        static drogon::Task<bool> remove(
            const std::string& id,
            const std::string& tenantId
        );

        // -------------------------------------------------------------------------
        // ServiceMetadata
        // -------------------------------------------------------------------------
        static drogon::Task<std::vector<ServiceMetadata>> findServicesByParticipantId(
            const std::string& participantId,
            const std::string& tenantId
        );

        static drogon::Task<std::optional<ServiceMetadata>> findServiceById(
            const std::string& serviceId,
            const std::string& participantId
        );

        static drogon::Task<bool> serviceExistsByDocType(
            const std::string& participantId,
            const std::string& docTypeScheme,
            const std::string& docTypeValue
        );

        static drogon::Task<std::string> createService(
            const std::string& participantId,
            const std::string& docTypeScheme,
            const std::string& docTypeValue
        );

        static drogon::Task<bool> removeService(
            const std::string& serviceId,
            const std::string& participantId
        );

        // -------------------------------------------------------------------------
        // Processes
        // -------------------------------------------------------------------------
        static drogon::Task<std::string> createProcess(
            const std::string& serviceMetadataId,
            const std::string& processScheme,
            const std::string& processValue
        );

        static drogon::Task<bool> removeProcess(
            const std::string& processId,
            const std::string& serviceMetadataId
        );

        // -------------------------------------------------------------------------
        // Endpoints
        // -------------------------------------------------------------------------
        static drogon::Task<std::string> createEndpoint(
            const std::string& processId,
            const std::string& transportProfile,
            const std::string& endpointUrl,
            const std::string& apCertificatePem,
            bool               requireBls,
            const std::string& serviceActivationDate,
            const std::string& serviceExpirationDate,
            const std::string& technicalContactUrl,
            const std::string& technicalInformationUrl,
            const std::string& serviceDescription
        );

        static drogon::Task<bool> removeEndpoint(
            const std::string& endpointId,
            const std::string& processId
        );
    };

} // namespace SMPPlusPlus
