//
// Created by Saadat Baig on 28.06.26.
//
#pragma once

#include <drogon/HttpFilter.h>
#include <string>


namespace SMPPlusPlus {

    /**
     * BasicAuthFilter - HTTP Basic Auth for all management API routes.
     *
     * Expects: Authorization: Basic <base64(username:password)>
     *
     * On success: attaches auth metadata to request attributes:
     *   "auth_user_id"   -> std::string UUID
     *   "auth_user_role" -> std::string "CORE_ADMIN" | "MANAGEMENT"
     *
     * On failure: returns 401 with WWW-Authenticate header.
     *
     * Threading: bcrypt verification is CPU-bound (~100ms). We offload it
     * to a detached thread and invoke the filter callbacks from there.
     * Drogon's FilterCallback and FilterChainCallback are thread-safe to
     * call from any thread.
     *
     * Register on routes via:
     *   METHOD_ADD(Ctrl::handler, "/path", Get, "SMPPlusPlus::BasicAuthFilter");
     */
    class BasicAuthFilter : public drogon::HttpFilter<BasicAuthFilter> {
    public:
        void doFilter(
            const drogon::HttpRequestPtr& req,
            drogon::FilterCallback&&      fcb,
            drogon::FilterChainCallback&& fccb
        ) override;

    private:
        static std::pair<std::string, std::string> decodeBasicAuth(
            const std::string& headerValue
        );

        static drogon::HttpResponsePtr unauthorizedResponse();
    };

} // namespace SMPPlusPlus