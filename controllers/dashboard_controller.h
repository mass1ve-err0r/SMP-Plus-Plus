//
// Created by Saadat Baig on 08.07.26.
//
#pragma once

#include <drogon/HttpController.h>


namespace SMPPlusPlus
{
    /**
     * DashboardController - Management dashboard summary.
     *
     * GET /api/management/dashboard
     *   Returns aggregate counts scoped to the caller.
     *   CORE_ADMIN sees counts across everything.
     *   MANAGEMENT sees counts scoped to their granted tenants.
     *
     * Response:
     *   {
     *     "tenants":      3,
     *     "participants": 18,
     *     "services":     42,
     *     "endpoints":    42
     *   }
     */
    class DashboardController : public drogon::HttpController<DashboardController> {
    public:
        METHOD_LIST_BEGIN
            ADD_METHOD_TO(DashboardController::getCounts,
                "/api/management/dashboard",
                drogon::Get,
                "SMPPlusPlus::SessionOrBasicAuthFilter");
        METHOD_LIST_END

        drogon::Task<> getCounts(
            drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback
        );
    };

} // namespace SMPPlusPlus
