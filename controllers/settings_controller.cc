//
// Created by Saadat Baig on 29.06.26.
//
#include "settings_controller.h"
#include "db/settings_repository.h"

#include <drogon/drogon.h>


namespace SMPPlusPlus
{
    namespace {

        // Keys that are managed by the system and should not be overwritten by users
        const std::vector<std::string> READONLY_KEYS = {
            "app.version"
        };

        drogon::HttpResponsePtr jsonOk(const Json::Value& body) {
            auto resp = drogon::HttpResponse::newHttpJsonResponse(body);
            resp->setStatusCode(drogon::k200OK);
            return resp;
        }

        drogon::HttpResponsePtr jsonError(drogon::HttpStatusCode code,
                                          const std::string& message) {
            Json::Value body;
            body["error"] = message;
            auto resp = drogon::HttpResponse::newHttpJsonResponse(body);
            resp->setStatusCode(code);
            return resp;
        }

        std::string authUserId(const drogon::HttpRequestPtr& req) {
            return req->getAttributes()->get<std::string>("auth_user_id");
        }

        Json::Value settingToJson(const Setting& s) {
            Json::Value v;
            v["key"]        = s.key;
            v["value"]      = s.value;
            v["updated_at"] = s.updatedAt;
            v["updated_by"] = s.updatedBy;
            return v;
        }

        bool isReadOnly(const std::string& key) {
            return std::find(READONLY_KEYS.begin(), READONLY_KEYS.end(), key)
                   != READONLY_KEYS.end();
        }

    } // anonymous namespace

    // GET /api/management/settings
    drogon::Task<> SettingsController::getAll(
        drogon::HttpRequestPtr req,
        std::function<void(const drogon::HttpResponsePtr&)> callback)
    {
        auto settings = co_await SettingsRepository::findAll();

        Json::Value body(Json::arrayValue);
        for (const auto& s : settings)
            body.append(settingToJson(s));

        callback(jsonOk(body));
    }

    // GET /api/management/settings/:key
    drogon::Task<> SettingsController::getOne(
        drogon::HttpRequestPtr req,
        std::function<void(const drogon::HttpResponsePtr&)> callback,
        std::string key)
    {
        auto setting = co_await SettingsRepository::findByKey(key);
        if (!setting) {
            callback(jsonError(drogon::k404NotFound,
                "Setting '" + key + "' not found"));
            co_return;
        }
        callback(jsonOk(settingToJson(*setting)));
    }

    // PUT /api/management/settings/:key
    // Body: { "value": "..." }
    drogon::Task<> SettingsController::upsert(
        drogon::HttpRequestPtr req,
        std::function<void(const drogon::HttpResponsePtr&)> callback,
        std::string key)
    {
        if (isReadOnly(key)) {
            callback(jsonError(drogon::k403Forbidden,
                "Setting '" + key + "' is read-only"));
            co_return;
        }

        const auto body = req->getJsonObject();
        if (!body || !body->isMember("value")) {
            callback(jsonError(drogon::k400BadRequest,
                "Field 'value' is required"));
            co_return;
        }

        const std::string value = (*body)["value"].asString();

        if (!co_await SettingsRepository::upsert(key, value, authUserId(req))) {
            callback(jsonError(drogon::k500InternalServerError,
                "Failed to update setting"));
            co_return;
        }

        auto updated = co_await SettingsRepository::findByKey(key);
        callback(jsonOk(settingToJson(*updated)));
    }

} // namespace SMPPlusPlus
