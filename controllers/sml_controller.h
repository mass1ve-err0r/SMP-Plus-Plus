//
// Created by Saadat Baig on 29.06.26.
//
#pragma once

#include <drogon/HttpController.h>


namespace SMPPlusPlus
{
    /**
     * SmlController - SML configuration management.
     *
     * All authenticated users (CORE_ADMIN and MANAGEMENT) have full access.
     * The UI renders edit controls based on role — that is a frontend concern.
     *
     * GET    /api/management/sml                  → list all
     * GET    /api/management/sml/:id              → get one
     * POST   /api/management/sml                  → create
     * PUT    /api/management/sml/:id              → update
     * DELETE /api/management/sml/:id              → delete (cannot delete primary)
     * PATCH  /api/management/sml/:id/enable       → enable
     * PATCH  /api/management/sml/:id/disable      → disable
     * PATCH  /api/management/sml/:id/primary      → set as primary
     */
    class SmlController : public drogon::HttpController<SmlController> {
    public:
        METHOD_LIST_BEGIN
            ADD_METHOD_TO(SmlController::getAll,
                "/api/management/sml",
                drogon::Get, "SMPPlusPlus::SessionOrBasicAuthFilter");

            ADD_METHOD_TO(SmlController::getOne,
                "/api/management/sml/{id}",
                drogon::Get, "SMPPlusPlus::SessionOrBasicAuthFilter");

            ADD_METHOD_TO(SmlController::create,
                "/api/management/sml",
                drogon::Post, "SMPPlusPlus::SessionOrBasicAuthFilter");

            ADD_METHOD_TO(SmlController::update,
                "/api/management/sml/{id}",
                drogon::Put, "SMPPlusPlus::SessionOrBasicAuthFilter");

            ADD_METHOD_TO(SmlController::remove,
                "/api/management/sml/{id}",
                drogon::Delete, "SMPPlusPlus::SessionOrBasicAuthFilter");

            ADD_METHOD_TO(SmlController::enable,
                "/api/management/sml/{id}/enable",
                drogon::Patch, "SMPPlusPlus::SessionOrBasicAuthFilter");

            ADD_METHOD_TO(SmlController::disable,
                "/api/management/sml/{id}/disable",
                drogon::Patch, "SMPPlusPlus::SessionOrBasicAuthFilter");

            ADD_METHOD_TO(SmlController::setPrimary,
                "/api/management/sml/{id}/primary",
                drogon::Patch, "SMPPlusPlus::SessionOrBasicAuthFilter");
        METHOD_LIST_END

        drogon::Task<> getAll(drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback);

        drogon::Task<> getOne(drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback,
            std::string id);

        drogon::Task<> create(drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback);

        drogon::Task<> update(drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback,
            std::string id);

        drogon::Task<> remove(drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback,
            std::string id);

        drogon::Task<> enable(drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback,
            std::string id);

        drogon::Task<> disable(drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback,
            std::string id);

        drogon::Task<> setPrimary(drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback,
            std::string id);
    };

} // namespace SMPPlusPlus
