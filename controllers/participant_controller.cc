//
// Created by Saadat Baig on 29.06.26.
//
#include "participant_controller.h"
#include "db/participant_repository.h"
#include "db/tenant_repository.h"

#include <drogon/drogon.h>


namespace SMPPlusPlus
{
    namespace {

        constexpr std::string_view DEFAULT_SCHEME = "iso6523-actorid-upis";

        drogon::HttpResponsePtr jsonOk(const Json::Value& body) {
            auto resp = drogon::HttpResponse::newHttpJsonResponse(body);
            resp->setStatusCode(drogon::k200OK);
            return resp;
        }

        drogon::HttpResponsePtr jsonCreated(const Json::Value& body) {
            auto resp = drogon::HttpResponse::newHttpJsonResponse(body);
            resp->setStatusCode(drogon::k201Created);
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

        Json::Value participantToJson(const Participant& p) {
            Json::Value v;
            v["id"]                = p.id;
            v["tenant_id"]         = p.tenantId;
            v["identifier_scheme"] = p.identifierScheme;
            v["identifier_value"]  = p.identifierValue;
            // Full Peppol identifier in canonical form for convenience
            v["peppol_id"]         = p.identifierScheme + "::" + p.identifierValue;
            v["created_at"]        = p.createdAt;
            v["created_by"]        = p.createdBy;
            return v;
        }

        // Verify the requesting user has access to the given tenant
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

    } // anonymous namespace

    // -----------------------------------------------------------------------------
    // GET /api/management/tenants/:tenantId/participants
    // -----------------------------------------------------------------------------
    drogon::Task<> ParticipantController::getAll(
        drogon::HttpRequestPtr req,
        std::function<void(const drogon::HttpResponsePtr&)> callback,
        std::string tenantId)
    {
        if (!co_await hasAccessToTenant(req, tenantId)) {
            callback(jsonError(drogon::k403Forbidden, "Access denied"));
            co_return;
        }

        auto participants = co_await ParticipantRepository::findByTenantId(tenantId);

        Json::Value body(Json::arrayValue);
        for (const auto& p : participants)
            body.append(participantToJson(p));

        callback(jsonOk(body));
    }

    // -----------------------------------------------------------------------------
    // GET /api/management/tenants/:tenantId/participants/:id
    // -----------------------------------------------------------------------------
    drogon::Task<> ParticipantController::getOne(
        drogon::HttpRequestPtr req,
        std::function<void(const drogon::HttpResponsePtr&)> callback,
        std::string tenantId,
        std::string id)
    {
        if (!co_await hasAccessToTenant(req, tenantId)) {
            callback(jsonError(drogon::k403Forbidden, "Access denied"));
            co_return;
        }

        auto participant = co_await ParticipantRepository::findById(id, tenantId);
        if (!participant) {
            callback(jsonError(drogon::k404NotFound, "Participant not found"));
            co_return;
        }

        callback(jsonOk(participantToJson(*participant)));
    }

    // -----------------------------------------------------------------------------
    // POST /api/management/tenants/:tenantId/participants
    // Body: {
    //   "identifier_scheme": "iso6523-actorid-upis",  // optional, defaults to ^
    //   "identifier_value":  "0088:123456789"          // required
    // }
    // -----------------------------------------------------------------------------
    drogon::Task<> ParticipantController::create(
        drogon::HttpRequestPtr req,
        std::function<void(const drogon::HttpResponsePtr&)> callback,
        std::string tenantId)
    {
        if (!co_await hasAccessToTenant(req, tenantId)) {
            callback(jsonError(drogon::k403Forbidden, "Access denied"));
            co_return;
        }

        // Verify tenant exists
        auto tenant = co_await TenantRepository::findById(tenantId);
        if (!tenant) {
            callback(jsonError(drogon::k404NotFound, "Tenant not found"));
            co_return;
        }

        const auto body = req->getJsonObject();
        if (!body || !body->isMember("identifier_value") ||
            (*body)["identifier_value"].asString().empty())
        {
            callback(jsonError(drogon::k400BadRequest,
                "Field 'identifier_value' is required"));
            co_return;
        }

        const std::string scheme = body->isMember("identifier_scheme") &&
                                   !(*body)["identifier_scheme"].asString().empty()
            ? (*body)["identifier_scheme"].asString()
            : std::string(DEFAULT_SCHEME);

        const std::string value = (*body)["identifier_value"].asString();

        // Duplicate check
        if (co_await ParticipantRepository::existsByIdentifier(tenantId, scheme, value)) {
            callback(jsonError(drogon::k409Conflict,
                "Participant '" + scheme + "::" + value +
                "' already exists under this tenant"));
            co_return;
        }

        const std::string newId = co_await ParticipantRepository::create(
            tenantId, scheme, value, authUserId(req)
        );

        if (newId.empty()) {
            callback(jsonError(drogon::k500InternalServerError,
                "Failed to create participant"));
            co_return;
        }

        auto created = co_await ParticipantRepository::findById(newId, tenantId);
        if (!created) {
            callback(jsonError(drogon::k500InternalServerError,
                "Participant created but not found"));
            co_return;
        }

        callback(jsonCreated(participantToJson(*created)));
    }

    // -----------------------------------------------------------------------------
    // DELETE /api/management/tenants/:tenantId/participants/:id
    // Cascades to service_metadata -> processes -> endpoints
    // -----------------------------------------------------------------------------
    drogon::Task<> ParticipantController::remove(
        drogon::HttpRequestPtr req,
        std::function<void(const drogon::HttpResponsePtr&)> callback,
        std::string tenantId,
        std::string id)
    {
        if (!co_await hasAccessToTenant(req, tenantId)) {
            callback(jsonError(drogon::k403Forbidden, "Access denied"));
            co_return;
        }

        auto existing = co_await ParticipantRepository::findById(id, tenantId);
        if (!existing) {
            callback(jsonError(drogon::k404NotFound, "Participant not found"));
            co_return;
        }

        if (!co_await ParticipantRepository::remove(id, tenantId)) {
            callback(jsonError(drogon::k500InternalServerError,
                "Failed to delete participant"));
            co_return;
        }

        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::k204NoContent);
        callback(resp);
    }

} // namespace SMPPlusPlus
