//
// Created by Saadat Baig on 07.07.26.
//
#include "auth_controller.h"
#include "filters/session_or_basic_auth_filter.h"
#include "db/session_repository.h"
#include "db/user_repository.h"
#include "utils/crypto_utils.h"
#include "utils/environment.h"
#include "utils/sanitize_or_reject.h"

#include <drogon/drogon.h>


namespace SMPPlusPlus
{
    namespace {

        constexpr int SLIDING_WINDOW_HOURS = 8;
        constexpr int MAX_LIFETIME_HOURS   = 168; // 7 days

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

        Json::Value userToJson(const User& u) {
            Json::Value v;
            v["id"]        = u.id;
            v["username"]  = u.username;
            v["role"]      = u.role;
            v["is_active"] = u.isActive;
            return v;
        }

        Json::Value sessionToJson(const Session& s, bool isCurrent) {
            Json::Value v;
            v["id"]           = s.id;
            v["created_at"]   = s.createdAt;
            v["expires_at"]   = s.expiresAt;
            v["last_used_at"] = s.lastUsedAt;
            v["user_agent"]   = s.userAgent;
            v["ip_address"]   = s.ipAddress;
            v["is_current"]   = isCurrent;
            // Token intentionally omitted - only the browser holds it via HttpOnly cookie
            return v;
        }

        // Resolve the real client IP.
        // When behind a reverse proxy, X-Forwarded-For / X-Real-IP contain the
        // real client IP; getPeerAddr() returns the proxy's internal IP.
        // Otherwise, use getPeerAddr() directly.
        std::string resolveClientIp(const drogon::HttpRequestPtr& req) {
            if (Environment::behindProxy()) {
                // X-Forwarded-For may contain a comma-separated chain -
                // the first entry is the original client.
                const std::string fwd = req->getHeader("X-Forwarded-For");
                if (!fwd.empty()) {
                    const auto comma = fwd.find(',');
                    std::string ip = (comma == std::string::npos)
                                         ? fwd : fwd.substr(0, comma);
                    // Trim leading/trailing whitespace
                    while (!ip.empty() && std::isspace(
                        static_cast<unsigned char>(ip.front()))) ip.erase(0, 1);
                    while (!ip.empty() && std::isspace(
                        static_cast<unsigned char>(ip.back()))) ip.pop_back();
                    if (!ip.empty()) return ip;
                }
                const std::string realIp = req->getHeader("X-Real-IP");
                if (!realIp.empty()) return realIp;
            }
            return req->getPeerAddr().toIp();
        }

        // Build the Set-Cookie header value.
        // HttpOnly + SameSite=Strict + Path=/, Secure unless in dev mode.
        // Note: Secure flag is set based on !devMode, NOT on behindProxy.
        // Behind a proxy the client-to-edge hop is still HTTPS, so Secure must stay.
        std::string buildSessionCookie(
            const std::string& token,
            int                maxAgeSeconds)
        {
            std::string cookie = std::string(SessionOrBasicAuthFilter::COOKIE_NAME)
                               + "=" + token
                               + "; HttpOnly"
                               + "; SameSite=Strict"
                               + "; Path=/"
                               + "; Max-Age=" + std::to_string(maxAgeSeconds);

            if (!Environment::devMode()) {
                cookie += "; Secure";
            }
            return cookie;
        }

        // Build the Set-Cookie header to clear the session cookie.
        std::string buildClearCookie() {
            std::string cookie = std::string(SessionOrBasicAuthFilter::COOKIE_NAME)
                               + "="
                               + "; HttpOnly"
                               + "; SameSite=Strict"
                               + "; Path=/"
                               + "; Max-Age=0";
            if (!Environment::devMode()) {
                cookie += "; Secure";
            }
            return cookie;
        }

    } // anonymous namespace

    // -----------------------------------------------------------------------------
    // POST /api/auth/login
    // -----------------------------------------------------------------------------
    drogon::Task<> AuthController::login(
        drogon::HttpRequestPtr req,
        std::function<void(const drogon::HttpResponsePtr&)> callback)
    {
        const auto body = req->getJsonObject();
        if (!body ||
            !body->isMember("username") ||
            !body->isMember("password") ||
            (*body)["username"].asString().empty() ||
            (*body)["password"].asString().empty())
        {
            callback(jsonError(drogon::k400BadRequest,
                "Fields 'username' and 'password' are required"));
            co_return;
        }

        if (!sanitizeOrReject(*body, callback))
        {
            co_return;
        }

        const std::string username = (*body)["username"].asString();
        const std::string password = (*body)["password"].asString();

        auto user = co_await UserRepository::findByUsername(username);
        if (!user) {
            callback(jsonError(drogon::k401Unauthorized, "Invalid credentials"));
            co_return;
        }

        auto verifyResult = CryptoUtils::bcryptVerify(password, user->passwordHash);
        if (!verifyResult || !(*verifyResult)) {
            callback(jsonError(drogon::k401Unauthorized, "Invalid credentials"));
            co_return;
        }

        const std::string userAgent = req->getHeader("User-Agent");
        const std::string ipAddress = resolveClientIp(req);

        const std::string token = co_await SessionRepository::create(
            user->id, userAgent, ipAddress, SLIDING_WINDOW_HOURS
        );

        if (token.empty()) {
            callback(jsonError(drogon::k500InternalServerError,
                "Failed to create session"));
            co_return;
        }

        Json::Value response = userToJson(*user);
        auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
        resp->setStatusCode(drogon::k200OK);
        resp->addHeader("Set-Cookie",
            buildSessionCookie(token, SLIDING_WINDOW_HOURS * 3600));

        callback(resp);
    }

    // -----------------------------------------------------------------------------
    // POST /api/auth/logout
    // -----------------------------------------------------------------------------
    drogon::Task<> AuthController::logout(
        drogon::HttpRequestPtr req,
        std::function<void(const drogon::HttpResponsePtr&)> callback)
    {
        // If authenticated via session cookie, revoke the session.
        // If authenticated via Basic Auth, nothing to revoke - just clear cookie.
        const std::string sessionToken = req->getCookie(
            SessionOrBasicAuthFilter::COOKIE_NAME);

        if (!sessionToken.empty()) {
            co_await SessionRepository::revokeByToken(sessionToken);
        }

        Json::Value body;
        body["message"] = "Logged out";

        auto resp = drogon::HttpResponse::newHttpJsonResponse(body);
        resp->setStatusCode(drogon::k200OK);
        resp->addHeader("Set-Cookie", buildClearCookie());

        callback(resp);
    }

    // -----------------------------------------------------------------------------
    // GET /api/auth/me
    // -----------------------------------------------------------------------------
    drogon::Task<> AuthController::me(
        drogon::HttpRequestPtr req,
        std::function<void(const drogon::HttpResponsePtr&)> callback)
    {
        auto user = co_await UserRepository::findById(authUserId(req));
        if (!user) {
            callback(jsonError(drogon::k404NotFound, "User not found"));
            co_return;
        }

        Json::Value body = userToJson(*user);
        body["auth_method"] = req->getAttributes()->get<std::string>("auth_method");
        callback(jsonOk(body));
    }

    // -----------------------------------------------------------------------------
    // GET /api/auth/sessions
    // -----------------------------------------------------------------------------
    drogon::Task<> AuthController::listSessions(
        drogon::HttpRequestPtr req,
        std::function<void(const drogon::HttpResponsePtr&)> callback)
    {
        const std::string userId       = authUserId(req);
        const std::string currentToken = req->getCookie(
            SessionOrBasicAuthFilter::COOKIE_NAME);

        auto sessions = co_await SessionRepository::findByUserId(userId);

        Json::Value body(Json::arrayValue);
        for (const auto& s : sessions) {
            const bool isCurrent = (!currentToken.empty() && s.token == currentToken);
            body.append(sessionToJson(s, isCurrent));
        }
        callback(jsonOk(body));
    }

    // -----------------------------------------------------------------------------
    // DELETE /api/auth/sessions/:id
    // -----------------------------------------------------------------------------
    drogon::Task<> AuthController::revokeSession(
        drogon::HttpRequestPtr req,
        std::function<void(const drogon::HttpResponsePtr&)> callback,
        std::string id)
    {
        const std::string userId = authUserId(req);

        if (!co_await SessionRepository::revokeByIdForUser(id, userId)) {
            callback(jsonError(drogon::k404NotFound,
                "Session not found or does not belong to you"));
            co_return;
        }

        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::k204NoContent);
        callback(resp);
    }

} // namespace SMPPlusPlus
