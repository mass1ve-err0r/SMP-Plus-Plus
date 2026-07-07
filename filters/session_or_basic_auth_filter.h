//
// Created by Saadat Baig on 07.07.26.
//
#pragma once

#include <drogon/HttpFilter.h>


namespace SMPPlusPlus
{
    /**
     * SessionOrBasicAuthFilter - Accepts either a session cookie or HTTP Basic Auth.
     *
     * Authentication order:
     *   1. Check for smp_session cookie. If present and valid, use it.
     *      Extends the session (sliding window).
     *   2. Fall back to Authorization: Basic header.
     *
     * On success, attaches:
     *   - auth_user_id
     *   - auth_user_role
     *   - auth_method  ("session" or "basic")
     *
     * On failure, returns 401 Unauthorized.
     */
    class SessionOrBasicAuthFilter
        : public drogon::HttpFilter<SessionOrBasicAuthFilter> {
    public:
        void doFilter(
            const drogon::HttpRequestPtr&      req,
            drogon::FilterCallback&&           fcb,
            drogon::FilterChainCallback&&      fccb
        ) override;

        static drogon::HttpResponsePtr unauthorizedResponse();

        // Cookie name used across the app
        static constexpr const char* COOKIE_NAME = "smp_session";
    };

} // namespace SMPPlusPlus
