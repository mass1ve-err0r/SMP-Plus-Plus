//
// Created by Saadat Baig on 08.07.26.
//
#include "dashboard_controller.h"
#include "db/dashboard_repository.h"

#include <drogon/drogon.h>


namespace SMPPlusPlus
{
    namespace {

        drogon::HttpResponsePtr jsonOk(const Json::Value& body) {
            auto resp = drogon::HttpResponse::newHttpJsonResponse(body);
            resp->setStatusCode(drogon::k200OK);
            return resp;
        }

        bool isCoreAdmin(const drogon::HttpRequestPtr& req) {
            return req->getAttributes()->get<std::string>("auth_user_role") == "CORE_ADMIN";
        }

        std::string authUserId(const drogon::HttpRequestPtr& req) {
            return req->getAttributes()->get<std::string>("auth_user_id");
        }

    } // anonymous namespace

    drogon::Task<> DashboardController::getCounts(
        drogon::HttpRequestPtr req,
        std::function<void(const drogon::HttpResponsePtr&)> callback)
    {
        auto counts = co_await DashboardRepository::getCounts(
            authUserId(req),
            isCoreAdmin(req)
        );

        Json::Value body;
        body["tenants"]      = counts.tenants;
        body["participants"] = counts.participants;
        body["services"]     = counts.services;
        body["endpoints"]    = counts.endpoints;

        callback(jsonOk(body));
    }

} // namespace SMPPlusPlus
