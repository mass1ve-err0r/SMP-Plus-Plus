//
// Created by Saadat Baig on 08.07.26.
//
#pragma once

#include <string>
#include <drogon/drogon.h>


namespace SMPPlusPlus
{
    /**
     * DashboardRepository - Aggregate counts for the management dashboard.
     *
     * Scope rules:
     *   - CORE_ADMIN: counts across all tenants and everything under them.
     *   - MANAGEMENT: counts only tenants they have been granted access to,
     *                 plus the participants/services/endpoints under those.
     *
     * All counts are returned in a single query using CTEs so the endpoint
     * makes one round trip.
     */
    struct DashboardCounts {
        int tenants      = 0;
        int participants = 0;
        int services     = 0;
        int endpoints    = 0;
    };

    class DashboardRepository {
    public:
        DashboardRepository() = delete;

        /**
         * Get aggregate counts scoped to the calling user.
         *
         * @param userId       The authenticated user's id.
         * @param isCoreAdmin  If true, counts across all tenants.
         *                     If false, counts scoped to user_tenant_grants.
         */
        static drogon::Task<DashboardCounts> getCounts(
            const std::string& userId,
            bool               isCoreAdmin
        );
    };

} // namespace SMPPlusPlus
