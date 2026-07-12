//
// Created by Saadat Baig on 06.07.26.
//
#pragma once

#include "db/tenant_repository.h"

#include <drogon/HttpRequest.h>
#include <drogon/drogon.h>

#include <optional>
#include <string>
#include <string_view>


namespace SMPPlusPlus
{
    class AuthUtils
    {
    public:

        AuthUtils() = delete;

        static std::optional<std::string>
        authUserId(const drogon::HttpRequestPtr& req)
        {
            return requestStringAttribute(req, "auth_user_id");
        }

        static std::optional<std::string>
        authUserRole(const drogon::HttpRequestPtr& req)
        {
            return requestStringAttribute(req, "auth_user_role");
        }

        static bool
        isCoreAdmin(const drogon::HttpRequestPtr& req)
        {
            const auto role = authUserRole(req);
            return role.has_value() && *role == "CORE_ADMIN";
        }

        static drogon::Task<bool>
        hasAccessToTenant(const drogon::HttpRequestPtr& req, const std::string& tenantId)
        {
            if (tenantId.empty())
            {
                co_return false;
            }

            if (isCoreAdmin(req))
            {
                co_return true;
            }

            const auto userId = authUserId(req);
            if (!userId || userId->empty())
            {
                co_return false;
            }

            const auto granted = co_await TenantRepository::findByUserId(*userId);
            for (const auto& tenant : granted)
            {
                if (tenant.id == tenantId)
                {
                    co_return true;
                }
            }

            co_return false;
        }

    private:

        static std::optional<std::string>
        requestStringAttribute(const drogon::HttpRequestPtr& req, std::string_view key)
        {
            if (!req)
            {
                return std::nullopt;
            }

            try
            {
                return req->getAttributes()->get<std::string>(std::string(key));
            } catch (...) {
                return std::nullopt;
            }
        }

    };

}
