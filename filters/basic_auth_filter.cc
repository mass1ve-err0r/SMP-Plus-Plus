//
// Created by Saadat Baig on 28.06.26.
//
#include "basic_auth_filter.h"
#include "db/user_repository.h"
#include "utils/crypto_utils.h"

#include <drogon/drogon.h>


namespace SMPPlusPlus
{
    // -----------------------------------------------------------------------------
    // unauthorizedResponse
    // -----------------------------------------------------------------------------
    drogon::HttpResponsePtr BasicAuthFilter::unauthorizedResponse() {
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::k401Unauthorized);
        resp->addHeader("WWW-Authenticate", "Basic realm=\"smp++\"");
        resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
        resp->setBody("{\"error\":\"Unauthorized\"}");
        return resp;
    }

    // -----------------------------------------------------------------------------
    // decodeBasicAuth
    // Parses "Basic <base64(user:pass)>" -> {user, pass}
    // Returns {"", ""} on any parse failure.
    // -----------------------------------------------------------------------------
    std::pair<std::string, std::string> BasicAuthFilter::decodeBasicAuth(
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

    // -----------------------------------------------------------------------------
    // doFilter
    //
    // Flow:
    //  1. Extract + decode Authorization header -> {username, password}
    //  2. Offload to a detached thread:
    //     a. Look up user in DB via async coroutine (bridges back via promise)
    //     b. bcrypt verify password
    //     c. Call fccb (pass) or fcb(401) (fail)
    // -----------------------------------------------------------------------------
    void BasicAuthFilter::doFilter(
    const drogon::HttpRequestPtr& req,
    drogon::FilterCallback&&      fcb,
    drogon::FilterChainCallback&& fccb)
    {
        const std::string authHeader = req->getHeader("Authorization");
        if (authHeader.empty()) { fcb(unauthorizedResponse()); return; }

        auto [username, password] = decodeBasicAuth(authHeader);
        if (username.empty() || password.empty()) { fcb(unauthorizedResponse()); return; }

        auto sharedFcb = std::make_shared<drogon::FilterCallback>(std::move(fcb));

        auto db = drogon::app().getFastDbClient("default");
        db->execSqlAsync(
            "SELECT id, username, password_hash, role FROM users "
            "WHERE username = $1 AND is_active = true LIMIT 1",
            [password = std::move(password),
             sharedFcb,
             fccb     = std::move(fccb),
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
                fccb();
            },
            [sharedFcb](const drogon::orm::DrogonDbException& e) mutable {
                LOG_ERROR << "[BasicAuthFilter] DB error: " << e.base().what();
                (*sharedFcb)(unauthorizedResponse());
            },
            username
        );
    }

} // namespace SMPPlusPlus
