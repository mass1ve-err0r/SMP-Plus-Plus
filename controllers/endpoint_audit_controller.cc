//
// Created by Saadat Baig on 07.07.26.
//
#include "endpoint_audit_controller.h"
#include "db/endpoint_audit_repository.h"
#include "db/tenant_repository.h"
#include "utils/pagination.h"

#include <drogon/drogon.h>


namespace SMPPlusPlus
{
    namespace {

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

        bool isCoreAdmin(const drogon::HttpRequestPtr& req) {
            return req->getAttributes()->get<std::string>("auth_user_role") == "CORE_ADMIN";
        }

        std::string authUserId(const drogon::HttpRequestPtr& req) {
            return req->getAttributes()->get<std::string>("auth_user_id");
        }

        drogon::Task<bool> hasAccessToTenant(
            const drogon::HttpRequestPtr& req,
            const std::string& tenantId)
        {
            if (isCoreAdmin(req)) co_return true;
            auto granted = co_await TenantRepository::findByUserId(authUserId(req));
            for (const auto& t : granted)
                if (t.id == tenantId) co_return true;
            co_return false;
        }

        Json::Value rowToJson(const EndpointAuditRow& r) {
            Json::Value v;
            v["endpoint_id"]              = r.endpointId;
            v["endpoint_url"]             = r.endpointUrl;
            v["transport_profile"]        = r.transportProfile;
            v["require_bls"]              = r.requireBls;
            v["service_activation_date"]  = r.serviceActivationDate;
            v["service_expiration_date"]  = r.serviceExpirationDate;
            v["created_at"]               = r.createdAt;

            v["participant_id"]           = r.participantId;
            v["identifier_scheme"]        = r.identifierScheme;
            v["identifier_value"]         = r.identifierValue;
            v["participant_identifier"]   = r.identifierScheme + "::" + r.identifierValue;

            v["service_metadata_id"]      = r.serviceMetadataId;
            v["document_type_scheme"]     = r.documentTypeScheme;
            v["document_type_value"]      = r.documentTypeValue;

            v["process_id"]               = r.processId;
            v["process_scheme"]           = r.processScheme;
            v["process_value"]            = r.processValue;

            return v;
        }

    } // anonymous namespace

    // -----------------------------------------------------------------------------
    // GET /api/management/tenants/:tenantId/endpoints
    // Query params: ?limit&offset&orderBy&orderDirection&search
    // -----------------------------------------------------------------------------
    drogon::Task<> EndpointAuditController::listEndpoints(
        drogon::HttpRequestPtr req,
        std::function<void(const drogon::HttpResponsePtr&)> callback,
        std::string tenantId)
    {
        if (!co_await hasAccessToTenant(req, tenantId)) {
            callback(jsonError(drogon::k403Forbidden, "Access denied"));
            co_return;
        }

        const auto p = Pagination::fromRequest(
            req,
            {"identifier_value",
             "document_type_value",
             "process_value",
             "endpoint_url",
             "created_at"},
            "identifier_value"
        );

        auto page = co_await EndpointAuditRepository::findPagedByTenantId(
            tenantId,
            p.limit,
            p.offset,
            p.orderBy,
            p.orderDirection,
            p.search
        );

        Json::Value items(Json::arrayValue);
        for (const auto& row : page.items)
            items.append(rowToJson(row));

        Json::Value body;
        body["items"]  = items;
        body["total"]  = page.total;
        body["limit"]  = p.limit;
        body["offset"] = p.offset;

        callback(jsonOk(body));
    }

} // namespace SMPPlusPlus
