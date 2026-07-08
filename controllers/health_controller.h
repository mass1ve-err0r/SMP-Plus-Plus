//
// Created by Saadat Baig on 28.06.26.
//
//
// Created by Saadat Baig on 28.06.26.
//
#pragma once

#include <drogon/HttpController.h>


namespace SMPPlusPlus
{
    /**
     * HealthController - Liveness and auth verification endpoints.
     *
     * GET /health        → 200 OK, no auth required. For load balancers / uptime checks.
     * GET /health/auth   → 200 OK, requires Basic Auth via BasicAuthFilter.
     *                      Returns the authenticated user's id and role.
     *                      Use this to verify auth logic via Postman.
     */
    class HealthController : public drogon::HttpController<HealthController> {
    public:
        METHOD_LIST_BEGIN
            ADD_METHOD_TO(HealthController::health, "/api/health", drogon::Get);
            ADD_METHOD_TO(HealthController::healthAuth, "/api/health/auth", drogon::Get, "SMPPlusPlus::SessionOrBasicAuthFilter");
        METHOD_LIST_END

        void health(
            const drogon::HttpRequestPtr& req,
            std::function<void(const drogon::HttpResponsePtr&)>&& callback
        );

        void healthAuth(
            const drogon::HttpRequestPtr& req,
            std::function<void(const drogon::HttpResponsePtr&)>&& callback
        );
    };

} // namespace SMPPlusPlus
