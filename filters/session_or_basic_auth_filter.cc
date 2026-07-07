//
// Created by Saadat Baig on 07.07.26.
//
#include "session_or_basic_auth_filter.h"
#include "db/session_repository.h"
#include "db/user_repository.h"
#include "utils/crypto_utils.h"

#include <drogon/drogon.h>


namespace SMPPlusPlus
{
    namespace {

        std::pair<std::string, std::string> decodeBasicAuth(
            const std::string& headerValue)
        {
            constexpr std::string_view prefix = "Basic ";
            if (headerValue.size() <= prefix.size() ||
                headerValue.substr(0, prefix.size()) != prefix)
            {
                return {"", ""};
            }

            const std::string encoded = headerValue.substr(prefix.size());
            auto decoded = CryptoUtils::base64Decode(encoded);
            if (!decoded) return {"", ""};

            const std::string plain(
                reinterpret_cast<const char*>(decoded->data()),
                decoded->size()
            );

            const auto colon = plain.find(':');
            if (colon == std::string::npos) return {"", ""};

            return { plain.substr(0, colon), plain.substr(colon + 1) };
        }

    } // anonymous namespace

    drogon::HttpResponsePtr SessionOrBasicAuthFilter::unauthorizedResponse() {
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::k401Unauthorized);
        resp->addHeader("WWW-Authenticate", "Basic realm=\"smp++\"");
        resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
        resp->setBody("{\"error\":\"Unauthorized\"}");
        return resp;
    }

    void SessionOrBasicAuthFilter::doFilter(
        const drogon::HttpRequestPtr& req,
        drogon::FilterCallback&&      fcb,
        drogon::FilterChainCallback&& fccb)
    {
        // -------------------------------------------------------------------------
        // Path 1: Try session cookie first
        // -------------------------------------------------------------------------
        const std::string sessionToken = req->getCookie(COOKIE_NAME);

        if (!sessionToken.empty()) {
            // Run as coroutine: look up session, extend, attach attributes
            drogon::async_run([req,
                               fcb  = std::move(fcb),
                               fccb = std::move(fccb),
                               sessionToken]() mutable -> drogon::Task<>
            {
                auto session = co_await SessionRepository::findByToken(sessionToken);
                if (!session) {
                    // Cookie was set but session is invalid/expired — fall through
                    // is not straightforward here; simplest to just return 401.
                    // The frontend clears the cookie on 401.
                    fcb(unauthorizedResponse());
                    co_return;
                }

                auto user = co_await UserRepository::findById(session->userId);
                if (!user || !user->isActive) {
                    fcb(unauthorizedResponse());
                    co_return;
                }

                // Extend session (sliding window)
                co_await SessionRepository::extend(sessionToken);

                req->getAttributes()->insert("auth_user_id",   user->id);
                req->getAttributes()->insert("auth_user_role", user->role);
                req->getAttributes()->insert("auth_method",    std::string("session"));

                fccb();
            });
            return;
        }

        // -------------------------------------------------------------------------
        // Path 2: Fall back to HTTP Basic Auth
        // -------------------------------------------------------------------------
        const std::string authHeader = req->getHeader("Authorization");
        if (authHeader.empty()) {
            fcb(unauthorizedResponse());
            return;
        }

        auto [username, password] = decodeBasicAuth(authHeader);
        if (username.empty() || password.empty()) {
            fcb(unauthorizedResponse());
            return;
        }

        auto db = drogon::app().getFastDbClient("default");
        auto sharedFcb = std::make_shared<drogon::FilterCallback>(std::move(fcb));

        db->execSqlAsync(
            "SELECT id, username, password_hash, role FROM users "
            "WHERE username = $1 AND is_active = true LIMIT 1",
            [password = std::move(password),
             sharedFcb,
             fccb = std::move(fccb),
             req](const drogon::orm::Result& result) mutable {

                if (result.empty()) { (*sharedFcb)(unauthorizedResponse()); return; }

                std::string id           = result[0]["id"].as<std::string>();
                std::string role         = result[0]["role"].as<std::string>();
                std::string passwordHash = result[0]["password_hash"].as<std::string>();

                auto verifyResult = CryptoUtils::bcryptVerify(password, passwordHash);
                if (!verifyResult || !(*verifyResult)) {
                    (*sharedFcb)(unauthorizedResponse());
                    return;
                }

                req->getAttributes()->insert("auth_user_id",   id);
                req->getAttributes()->insert("auth_user_role", role);
                req->getAttributes()->insert("auth_method",    std::string("basic"));
                fccb();
            },
            [sharedFcb](const drogon::orm::DrogonDbException& e) mutable {
                LOG_ERROR << "[SessionOrBasicAuthFilter] DB error: "
                          << e.base().what();
                (*sharedFcb)(unauthorizedResponse());
            },
            username
        );
    }

} // namespace SMPPlusPlus
