//
// Created by Saadat Baig on 29.06.26.
//
#pragma once

#include <drogon/HttpController.h>


namespace SMPPlusPlus
{
    /**
     * UserController - Management user CRUD and tenant grant management.
     *
     * All routes require BasicAuthFilter.
     * Most routes are CORE_ADMIN only, exceptions noted below.
     *
     * GET    /api/management/users                          → list all (CORE_ADMIN)
     * GET    /api/management/users/:id                      → get one (CORE_ADMIN)
     * POST   /api/management/users                          → create (CORE_ADMIN)
     * PATCH  /api/management/users/:id/deactivate           → soft delete (CORE_ADMIN)
     * PATCH  /api/management/users/:id/reactivate           → reactivate (CORE_ADMIN)
     * DELETE /api/management/users/:id                      → hard delete (CORE_ADMIN)
     * PATCH  /api/management/users/:id/password             → change password
     *                                                          (CORE_ADMIN for any user,
     *                                                           MANAGEMENT for own only)
     * GET    /api/management/users/:id/grants               → list grants (CORE_ADMIN)
     * POST   /api/management/users/:id/grants               → add grant (CORE_ADMIN)
     * DELETE /api/management/users/:id/grants/:tenantId     → remove grant (CORE_ADMIN)
     */
    class UserController : public drogon::HttpController<UserController> {
    public:
        METHOD_LIST_BEGIN
            ADD_METHOD_TO(UserController::getAll,
                "/api/management/users",
                drogon::Get, "SMPPlusPlus::BasicAuthFilter");

            ADD_METHOD_TO(UserController::getOne,
                "/api/management/users/{id}",
                drogon::Get, "SMPPlusPlus::BasicAuthFilter");

            ADD_METHOD_TO(UserController::create,
                "/api/management/users",
                drogon::Post, "SMPPlusPlus::BasicAuthFilter");

            ADD_METHOD_TO(UserController::deactivate,
                "/api/management/users/{id}/deactivate",
                drogon::Patch, "SMPPlusPlus::BasicAuthFilter");

            ADD_METHOD_TO(UserController::reactivate,
                "/api/management/users/{id}/reactivate",
                drogon::Patch, "SMPPlusPlus::BasicAuthFilter");

            ADD_METHOD_TO(UserController::hardDelete,
                "/api/management/users/{id}",
                drogon::Delete, "SMPPlusPlus::BasicAuthFilter");

            ADD_METHOD_TO(UserController::changePassword,
                "/api/management/users/{id}/password",
                drogon::Patch, "SMPPlusPlus::BasicAuthFilter");

            ADD_METHOD_TO(UserController::getGrants,
                "/api/management/users/{id}/grants",
                drogon::Get, "SMPPlusPlus::BasicAuthFilter");

            ADD_METHOD_TO(UserController::addGrant,
                "/api/management/users/{id}/grants",
                drogon::Post, "SMPPlusPlus::BasicAuthFilter");

            ADD_METHOD_TO(UserController::removeGrant,
                "/api/management/users/{id}/grants/{tenantId}",
                drogon::Delete, "SMPPlusPlus::BasicAuthFilter");
        METHOD_LIST_END

        drogon::Task<> getAll(drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback);

        drogon::Task<> getOne(drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback,
            std::string id);

        drogon::Task<> create(drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback);

        drogon::Task<> deactivate(drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback,
            std::string id);

        drogon::Task<> reactivate(drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback,
            std::string id);

        drogon::Task<> hardDelete(drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback,
            std::string id);

        drogon::Task<> changePassword(drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback,
            std::string id);

        drogon::Task<> getGrants(drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback,
            std::string id);

        drogon::Task<> addGrant(drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback,
            std::string id);

        drogon::Task<> removeGrant(drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback,
            std::string id, std::string tenantId);
    };

} // namespace SMPPlusPlus
