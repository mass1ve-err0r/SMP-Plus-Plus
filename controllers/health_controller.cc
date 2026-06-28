//
// Created by Saadat Baig on 28.06.26.
//
#include "health_controller.h"

#include <drogon/drogon.h>


namespace SMPPlusPlus
{
    // -----------------------------------------------------------------------------
    // GET /health
    // Public - no auth required.
    // -----------------------------------------------------------------------------
    void HealthController::health(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::k200OK);
        resp->setBody("OK");
        callback(resp);
    }

    // -----------------------------------------------------------------------------
    // GET /health/auth
    // Protected by BasicAuthFilter - only reachable if auth passed.
    // Returns the authenticated user's id and role from request attributes.
    // -----------------------------------------------------------------------------
    void HealthController::healthAuth(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        const auto attrs = req->getAttributes();

        Json::Value body;
        body["status"]        = "ok";
        body["auth_user_id"]  = attrs->get<std::string>("auth_user_id");
        body["auth_user_role"]= attrs->get<std::string>("auth_user_role");

        auto resp = drogon::HttpResponse::newHttpJsonResponse(body);
        resp->setStatusCode(drogon::k200OK);
        callback(resp);
    }

} // namespace SMPPlusPlus
