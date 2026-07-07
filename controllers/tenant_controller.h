//
// Created by Saadat Baig on 28.06.26.
//
#pragma once

#include <drogon/HttpController.h>


namespace SMPPlusPlus
{
    /**
     * TenantController - Management API for tenants.
     *
     * All routes protected by BasicAuthFilter.
     * CORE_ADMIN can access all tenants.
     * MANAGEMENT users can only access tenants they have been granted.
     *
     * GET    /api/management/tenants          → list tenants (scoped by role)
     * GET    /api/management/tenants/:id      → get single tenant
     * POST   /api/management/tenants          → create tenant (CORE_ADMIN only)
     * PUT    /api/management/tenants/:id      → update tenant (CORE_ADMIN only)
     * DELETE /api/management/tenants/:id      → delete tenant (CORE_ADMIN only)
     */
    class TenantController : public drogon::HttpController<TenantController> {
    public:
        METHOD_LIST_BEGIN
            ADD_METHOD_TO(TenantController::getAll,
                "/api/management/tenants",
                drogon::Get,
                "SMPPlusPlus::SessionOrBasicAuthFilter");

            ADD_METHOD_TO(TenantController::getOne,
                "/api/management/tenants/{id}",
                drogon::Get,
                "SMPPlusPlus::SessionOrBasicAuthFilter");

            ADD_METHOD_TO(TenantController::create,
                "/api/management/tenants",
                drogon::Post,
                "SMPPlusPlus::SessionOrBasicAuthFilter");

            ADD_METHOD_TO(TenantController::update,
                "/api/management/tenants/{id}",
                drogon::Put,
                "SMPPlusPlus::SessionOrBasicAuthFilter");

            ADD_METHOD_TO(TenantController::remove,
                "/api/management/tenants/{id}",
                drogon::Delete,
                "SMPPlusPlus::SessionOrBasicAuthFilter");
        METHOD_LIST_END

        drogon::Task<> getAll(
            drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback
        );

        drogon::Task<> getOne(
            drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback,
            std::string id
        );

        drogon::Task<> create(
            drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback
        );

        drogon::Task<> update(
            drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback,
            std::string id
        );

        drogon::Task<> remove(
            drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback,
            std::string id
        );
    };

} // namespace SMPPlusPlus
