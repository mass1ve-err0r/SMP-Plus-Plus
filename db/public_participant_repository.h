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
     * PublicParticipantRepository - Public SMP directory queries.
     *
     * Unlike ParticipantRepository which is tenant-scoped for the management API,
     * this queries across all tenants and deduplicates by (scheme, value) so the
     * public directory shows one row per unique Peppol identifier.
     *
     * The tenant concept is an internal management concern and MUST NOT leak
     * through this API surface. No tenant IDs, tenant names, or any other
     * management-side details are ever returned here.
     */
    struct PublicParticipantRow {
        std::string identifierScheme;
        std::string identifierValue;
    };

    class PublicParticipantRepository {
    public:
        PublicParticipantRepository() = delete;

        struct PagedPublicParticipants {
            std::vector<PublicParticipantRow> items;
            int total = 0;
        };

        /**
         * Paginated distinct public participant directory.
         *
         * Search matches identifier_scheme or identifier_value case-insensitively.
         * orderBy MUST be one of (validated at controller):
         *   "identifier_value", "identifier_scheme"
         * orderDirection MUST be "ASC" or "DESC".
         */
        static drogon::Task<PagedPublicParticipants> findPagedDistinct(
            int                limit,
            int                offset,
            const std::string& orderBy,
            const std::string& orderDirection,
            const std::string& search
        );
    };

} // namespace SMPPlusPlus
