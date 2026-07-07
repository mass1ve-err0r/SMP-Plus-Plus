//
// Created by Saadat Baig on 07.07.26.
//
#pragma once

#include <drogon/HttpController.h>


namespace SMPPlusPlus
{
    /**
     * AuthController - Session-based authentication for the browser UI.
     *
     * POST   /api/auth/login          → public, exchange credentials for a session cookie
     * POST   /api/auth/logout         → auth, revoke current session
     * GET    /api/auth/me             → auth, current user info
     * GET    /api/auth/sessions       → auth, list own active sessions
     * DELETE /api/auth/sessions/:id   → auth, revoke a specific own session
     *
     * The Bruno / curl workflow using Basic Auth is unchanged - both auth methods
     * are accepted by SessionOrBasicAuthFilter on protected routes.
     */
    class AuthController : public drogon::HttpController<AuthController> {
    public:
        METHOD_LIST_BEGIN
            ADD_METHOD_TO(AuthController::login,
                "/api/auth/login",
                drogon::Post);

            ADD_METHOD_TO(AuthController::logout,
                "/api/auth/logout",
                drogon::Post, "SMPPlusPlus::SessionOrBasicAuthFilter");

            ADD_METHOD_TO(AuthController::me,
                "/api/auth/me",
                drogon::Get, "SMPPlusPlus::SessionOrBasicAuthFilter");

            ADD_METHOD_TO(AuthController::listSessions,
                "/api/auth/sessions",
                drogon::Get, "SMPPlusPlus::SessionOrBasicAuthFilter");

            ADD_METHOD_TO(AuthController::revokeSession,
                "/api/auth/sessions/{id}",
                drogon::Delete, "SMPPlusPlus::SessionOrBasicAuthFilter");
        METHOD_LIST_END

        // POST /api/auth/login
        // Body: { "username": "...", "password": "..." }
        // Sets HttpOnly cookie on success.
        drogon::Task<> login(drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback);

        // POST /api/auth/logout
        drogon::Task<> logout(drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback);

        // GET /api/auth/me
        drogon::Task<> me(drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback);

        // GET /api/auth/sessions
        drogon::Task<> listSessions(drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback);

        // DELETE /api/auth/sessions/:id
        drogon::Task<> revokeSession(drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback,
            std::string id);
    };

} // namespace SMPPlusPlus
