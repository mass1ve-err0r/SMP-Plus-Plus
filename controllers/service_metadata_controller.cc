//
// Created by Saadat Baig on 29.06.26.
//
#include "service_metadata_controller.h"
#include "db/participant_repository.h"
#include "db/tenant_repository.h"
#include "utils/crypto_utils.h"

#include <drogon/drogon.h>


namespace SMPPlusPlus
{
    namespace {

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

        drogon::Task<bool> hasAccessToTenant(
            const drogon::HttpRequestPtr& req,
            const std::string& tenantId)
        {
            if (isCoreAdmin(req)) co_return true;
            auto granted = co_await TenantRepository::findByUserId(
                authUserId(req));
            for (const auto& t : granted)
                if (t.id == tenantId) co_return true;
            co_return false;
        }

        Json::Value endpointToJson(const Endpoint& e) {
            Json::Value v;
            v["id"]                        = e.id;
            v["process_id"]                = e.processId;
            v["transport_profile"]         = e.transportProfile;
            v["endpoint_url"]              = e.endpointUrl;
            v["ap_certificate_pem"]        = e.apCertificatePem;
            v["require_bls"]               = e.requireBls;
            v["service_activation_date"]   = e.serviceActivationDate;
            v["service_expiration_date"]   = e.serviceExpirationDate;
            v["technical_contact_url"]     = e.technicalContactUrl;
            v["technical_information_url"] = e.technicalInformationUrl;
            v["service_description"]       = e.serviceDescription;
            v["created_at"]                = e.createdAt;
            return v;
        }

        Json::Value processToJson(const Process& p) {
            Json::Value v;
            v["id"]                  = p.id;
            v["service_metadata_id"] = p.serviceMetadataId;
            v["process_scheme"]      = p.processScheme;
            v["process_value"]       = p.processValue;
            v["created_at"]          = p.createdAt;
            v["endpoints"]           = Json::arrayValue;
            for (const auto& e : p.endpoints)
                v["endpoints"].append(endpointToJson(e));
            return v;
        }

        Json::Value serviceToJson(const ServiceMetadata& s) {
            Json::Value v;
            v["id"]                   = s.id;
            v["participant_id"]       = s.participantId;
            v["document_type_scheme"] = s.documentTypeScheme;
            v["document_type_value"]  = s.documentTypeValue;
            v["created_at"]           = s.createdAt;
            v["processes"]            = Json::arrayValue;
            for (const auto& p : s.processes)
                v["processes"].append(processToJson(p));
            return v;
        }

        // Validate PEM is a parseable X.509 certificate
        bool isValidPem(const std::string& pem) {
            auto result = CryptoUtils::certFingerprint(pem);
            return result.has_value();
        }

    } // anonymous namespace

    // =============================================================================
    // Service Metadata
    // =============================================================================

    drogon::Task<> ServiceMetadataController::getServices(
        drogon::HttpRequestPtr req,
        std::function<void(const drogon::HttpResponsePtr&)> callback,
        std::string tenantId,
        std::string participantId)
    {
        if (!co_await hasAccessToTenant(req, tenantId)) {
            callback(jsonError(drogon::k403Forbidden, "Access denied"));
            co_return;
        }

        auto services = co_await ParticipantRepository::findServicesByParticipantId(
            participantId, tenantId);

        Json::Value body(Json::arrayValue);
        for (const auto& s : services)
            body.append(serviceToJson(s));

        callback(jsonOk(body));
    }

    drogon::Task<> ServiceMetadataController::getService(
        drogon::HttpRequestPtr req,
        std::function<void(const drogon::HttpResponsePtr&)> callback,
        std::string tenantId,
        std::string participantId,
        std::string serviceId)
    {
        if (!co_await hasAccessToTenant(req, tenantId)) {
            callback(jsonError(drogon::k403Forbidden, "Access denied"));
            co_return;
        }

        auto service = co_await ParticipantRepository::findServiceById(
            serviceId, participantId);
        if (!service) {
            callback(jsonError(drogon::k404NotFound, "Service not found"));
            co_return;
        }

        callback(jsonOk(serviceToJson(*service)));
    }

    // POST body: {
    //   "document_type_scheme": "busdox-docid-qns",     // optional, defaults to ^
    //   "document_type_value":  "urn:oasis:names:..."    // required
    // }
    drogon::Task<> ServiceMetadataController::createService(
        drogon::HttpRequestPtr req,
        std::function<void(const drogon::HttpResponsePtr&)> callback,
        std::string tenantId,
        std::string participantId)
    {
        if (!co_await hasAccessToTenant(req, tenantId)) {
            callback(jsonError(drogon::k403Forbidden, "Access denied"));
            co_return;
        }

        // Verify participant belongs to tenant
        auto participant = co_await ParticipantRepository::findById(
            participantId, tenantId);
        if (!participant) {
            callback(jsonError(drogon::k404NotFound, "Participant not found"));
            co_return;
        }

        const auto body = req->getJsonObject();
        if (!body || !body->isMember("document_type_value") ||
            (*body)["document_type_value"].asString().empty())
        {
            callback(jsonError(drogon::k400BadRequest,
                "Field 'document_type_value' is required"));
            co_return;
        }

        const std::string scheme = body->isMember("document_type_scheme") &&
                                   !(*body)["document_type_scheme"].asString().empty()
            ? (*body)["document_type_scheme"].asString()
            : "busdox-docid-qns";

        const std::string value = (*body)["document_type_value"].asString();

        if (co_await ParticipantRepository::serviceExistsByDocType(
                participantId, scheme, value)) {
            callback(jsonError(drogon::k409Conflict,
                "Service metadata for this document type already exists"));
            co_return;
                }

        const std::string newId = co_await ParticipantRepository::createService(
            participantId, scheme, value);

        if (newId.empty()) {
            callback(jsonError(drogon::k500InternalServerError,
                "Failed to create service metadata"));
            co_return;
        }

        auto created = co_await ParticipantRepository::findServiceById(
            newId, participantId);
        if (!created) {
            callback(jsonError(drogon::k500InternalServerError,
                "Service created but not found"));
            co_return;
        }

        callback(jsonCreated(serviceToJson(*created)));
    }

    drogon::Task<> ServiceMetadataController::removeService(
        drogon::HttpRequestPtr req,
        std::function<void(const drogon::HttpResponsePtr&)> callback,
        std::string tenantId,
        std::string participantId,
        std::string serviceId)
    {
        if (!co_await hasAccessToTenant(req, tenantId)) {
            callback(jsonError(drogon::k403Forbidden, "Access denied"));
            co_return;
        }

        auto existing = co_await ParticipantRepository::findServiceById(
            serviceId, participantId);
        if (!existing) {
            callback(jsonError(drogon::k404NotFound, "Service not found"));
            co_return;
        }

        if (!co_await ParticipantRepository::removeService(serviceId, participantId)) {
            callback(jsonError(drogon::k500InternalServerError,
                "Failed to delete service"));
            co_return;
        }

        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::k204NoContent);
        callback(resp);
    }

    // =============================================================================
    // Processes
    // =============================================================================

    // POST body: {
    //   "process_scheme": "cenbii-procid-ubl",            // optional, defaults to ^
    //   "process_value":  "urn:www.cenbii.eu:profile:..."  // required
    // }
    drogon::Task<> ServiceMetadataController::createProcess(
        drogon::HttpRequestPtr req,
        std::function<void(const drogon::HttpResponsePtr&)> callback,
        std::string tenantId,
        std::string participantId,
        std::string serviceId)
    {
        if (!co_await hasAccessToTenant(req, tenantId)) {
            callback(jsonError(drogon::k403Forbidden, "Access denied"));
            co_return;
        }

        auto service = co_await ParticipantRepository::findServiceById(
            serviceId, participantId);
        if (!service) {
            callback(jsonError(drogon::k404NotFound, "Service not found"));
            co_return;
        }

        const auto body = req->getJsonObject();
        if (!body || !body->isMember("process_value") ||
            (*body)["process_value"].asString().empty())
        {
            callback(jsonError(drogon::k400BadRequest,
                "Field 'process_value' is required"));
            co_return;
        }

        const std::string scheme = body->isMember("process_scheme") &&
                                   !(*body)["process_scheme"].asString().empty()
            ? (*body)["process_scheme"].asString()
            : "cenbii-procid-ubl";

        const std::string value = (*body)["process_value"].asString();

        const std::string newId = co_await ParticipantRepository::createProcess(
            serviceId, scheme, value);

        if (newId.empty()) {
            callback(jsonError(drogon::k500InternalServerError,
                "Failed to create process"));
            co_return;
        }

        // Re-fetch the full service to return updated state
        auto updated = co_await ParticipantRepository::findServiceById(
            serviceId, participantId);

        callback(jsonCreated(serviceToJson(*updated)));
    }

    drogon::Task<> ServiceMetadataController::removeProcess(
        drogon::HttpRequestPtr req,
        std::function<void(const drogon::HttpResponsePtr&)> callback,
        std::string tenantId,
        std::string participantId,
        std::string serviceId,
        std::string processId)
    {
        if (!co_await hasAccessToTenant(req, tenantId)) {
            callback(jsonError(drogon::k403Forbidden, "Access denied"));
            co_return;
        }

        if (!co_await ParticipantRepository::removeProcess(processId, serviceId)) {
            callback(jsonError(drogon::k404NotFound, "Process not found"));
            co_return;
        }

        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::k204NoContent);
        callback(resp);
    }

    // =============================================================================
    // Endpoints
    // =============================================================================

    // POST body: {
    //   "transport_profile":         "peppol-transport-as4-v2_0",  // optional, defaults to ^
    //   "endpoint_url":              "https://ap.example.com/as4", // required
    //   "ap_certificate_pem":        "-----BEGIN CERTIFICATE-----\n...", // required
    //   "require_bls":               false,                         // optional
    //   "service_activation_date":   "2025-01-01T00:00:00Z",       // optional
    //   "service_expiration_date":   "2027-01-01T00:00:00Z",       // optional
    //   "technical_contact_url":     "https://support.example.com",// optional
    //   "technical_information_url": "https://info.example.com",   // optional
    //   "service_description":       "Peppol AP for ACME"          // optional
    // }
    drogon::Task<> ServiceMetadataController::createEndpoint(
        drogon::HttpRequestPtr req,
        std::function<void(const drogon::HttpResponsePtr&)> callback,
        std::string tenantId,
        std::string participantId,
        std::string serviceId,
        std::string processId)
    {
        if (!co_await hasAccessToTenant(req, tenantId)) {
            callback(jsonError(drogon::k403Forbidden, "Access denied"));
            co_return;
        }

        const auto body = req->getJsonObject();
        if (!body) {
            callback(jsonError(drogon::k400BadRequest, "JSON body required"));
            co_return;
        }

        // Required fields
        if (!body->isMember("endpoint_url") ||
            (*body)["endpoint_url"].asString().empty())
        {
            callback(jsonError(drogon::k400BadRequest,
                "Field 'endpoint_url' is required"));
            co_return;
        }

        if (!body->isMember("ap_certificate_pem") ||
            (*body)["ap_certificate_pem"].asString().empty())
        {
            callback(jsonError(drogon::k400BadRequest,
                "Field 'ap_certificate_pem' is required"));
            co_return;
        }

        const std::string pem = (*body)["ap_certificate_pem"].asString();

        // Validate PEM is a parseable X.509 certificate
        if (!isValidPem(pem)) {
            callback(jsonError(drogon::k400BadRequest,
                "Field 'ap_certificate_pem' is not a valid X.509 PEM certificate"));
            co_return;
        }

        const std::string transportProfile =
            body->isMember("transport_profile") &&
            !(*body)["transport_profile"].asString().empty()
                ? (*body)["transport_profile"].asString()
                : "peppol-transport-as4-v2_0";

        const std::string endpointUrl    = (*body)["endpoint_url"].asString();
        const bool        requireBls     = body->isMember("require_bls")
                                            ? (*body)["require_bls"].asBool()
                                            : false;
        const std::string activationDate = body->isMember("service_activation_date")
                                            ? (*body)["service_activation_date"].asString()
                                            : "";
        const std::string expirationDate = body->isMember("service_expiration_date")
                                            ? (*body)["service_expiration_date"].asString()
                                            : "";
        const std::string contactUrl     = body->isMember("technical_contact_url")
                                            ? (*body)["technical_contact_url"].asString()
                                            : "";
        const std::string infoUrl        = body->isMember("technical_information_url")
                                            ? (*body)["technical_information_url"].asString()
                                            : "";
        const std::string description    = body->isMember("service_description")
                                            ? (*body)["service_description"].asString()
                                            : "";

        const std::string newId = co_await ParticipantRepository::createEndpoint(
            processId, transportProfile, endpointUrl, pem,
            requireBls, activationDate, expirationDate,
            contactUrl, infoUrl, description
        );

        if (newId.empty()) {
            callback(jsonError(drogon::k500InternalServerError,
                "Failed to create endpoint"));
            co_return;
        }

        // Return the full updated service for the UI to re-render
        auto updated = co_await ParticipantRepository::findServiceById(
            serviceId, participantId);

        callback(jsonCreated(serviceToJson(*updated)));
    }

    drogon::Task<> ServiceMetadataController::removeEndpoint(
        drogon::HttpRequestPtr req,
        std::function<void(const drogon::HttpResponsePtr&)> callback,
        std::string tenantId,
        std::string participantId,
        std::string serviceId,
        std::string processId,
        std::string endpointId)
    {
        if (!co_await hasAccessToTenant(req, tenantId)) {
            callback(jsonError(drogon::k403Forbidden, "Access denied"));
            co_return;
        }

        if (!co_await ParticipantRepository::removeEndpoint(endpointId, processId)) {
            callback(jsonError(drogon::k404NotFound, "Endpoint not found"));
            co_return;
        }

        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::k204NoContent);
        callback(resp);
    }

} // namespace SMPPlusPlus
