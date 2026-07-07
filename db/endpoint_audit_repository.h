//
// Created by Saadat Baig on 07.07.26.
//
#pragma once

#include <string>
#include <vector>
#include <drogon/drogon.h>


namespace SMPPlusPlus
{
    /**
     * EndpointAuditRow - Thin flat row for the admin "Endpoint List" audit view.
     *
     * Represents one endpoint denormalized with its participant, document type,
     * and process context so the admin UI can render a spreadsheet-style table
     * without needing to fetch each row's parents separately.
     *
     * Intentionally excluded from this thin row (fetch on detail-view instead):
     *   - ap_certificate_pem      (~2KB per row)
     *   - service_description     (free-form text, can be long)
     *   - technical_contact_url   (not needed for the audit list)
     *   - technical_information_url
     */
    struct EndpointAuditRow {
        // Endpoint fields
        std::string endpointId;
        std::string endpointUrl;
        std::string transportProfile;
        bool        requireBls = false;
        std::string serviceActivationDate;
        std::string serviceExpirationDate;
        std::string createdAt;

        // Denormalized parent context
        std::string participantId;
        std::string identifierScheme;
        std::string identifierValue;

        std::string serviceMetadataId;
        std::string documentTypeScheme;
        std::string documentTypeValue;

        std::string processId;
        std::string processScheme;
        std::string processValue;
    };

    class EndpointAuditRepository {
    public:
        EndpointAuditRepository() = delete;

        struct PagedEndpoints {
            std::vector<EndpointAuditRow> items;
            int total = 0;
        };

        /**
         * Paginated / searchable / sortable flat endpoint list scoped to a tenant.
         *
         * Search matches (case-insensitive substring) against:
         *   identifier_value, document_type_value, process_value, endpoint_url
         *
         * orderBy MUST be one of the whitelisted column names (enforced by
         * Pagination::fromRequest in the controller):
         *   "identifier_value", "document_type_value", "process_value",
         *   "endpoint_url", "created_at"
         *
         * orderDirection MUST be "ASC" or "DESC".
         */
        static drogon::Task<PagedEndpoints> findPagedByTenantId(
            const std::string& tenantId,
            int                limit,
            int                offset,
            const std::string& orderBy,
            const std::string& orderDirection,
            const std::string& search
        );
    };

} // namespace SMPPlusPlus
