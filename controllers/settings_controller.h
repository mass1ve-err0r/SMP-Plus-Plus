//
// Created by Saadat Baig on 29.06.26.
//
#pragma once

#include <drogon/HttpController.h>


namespace SMPPlusPlus
{
    /**
     * SettingsController - Global instance settings.
     *
     * All authenticated users can read and write settings.
     *
     * GET /api/management/settings          → list all
     * GET /api/management/settings/:key     → get one
     * PUT /api/management/settings/:key     → upsert value
     *
     * Well-known keys:
     *   smp.id                    The SMP operator ID registered with the SML
     *   sml.auto_register_enabled Whether participant creation triggers SML registration
     *   app.version               Application version (read-only, set by migrations)
     */
    class SettingsController : public drogon::HttpController<SettingsController> {
    public:
        METHOD_LIST_BEGIN
            ADD_METHOD_TO(SettingsController::getAll,
                "/api/management/settings",
                drogon::Get, "SMPPlusPlus::BasicAuthFilter");

            ADD_METHOD_TO(SettingsController::getOne,
                "/api/management/settings/{key}",
                drogon::Get, "SMPPlusPlus::BasicAuthFilter");

            ADD_METHOD_TO(SettingsController::upsert,
                "/api/management/settings/{key}",
                drogon::Put, "SMPPlusPlus::BasicAuthFilter");
        METHOD_LIST_END

        drogon::Task<> getAll(drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback);

        drogon::Task<> getOne(drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback,
            std::string key);

        drogon::Task<> upsert(drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback,
            std::string key);
    };

} // namespace SMPPlusPlus
