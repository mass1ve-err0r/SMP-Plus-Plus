//
// Created by Saadat Baig on 29.06.26.
//
#include "certificate_controller.h"
#include "db/certificate_repository.h"
#include "utils/crypto_utils.h"
#include "utils/environment.h"

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

        // Never send private key to client - strip it from responses
        Json::Value certToJson(const SmpCertificate& c) {
            Json::Value v;
            v["id"]          = c.id;
            v["label"]       = c.label;
            v["cert_pem"]    = c.certPem;
            v["fingerprint"] = c.fingerprint;
            v["valid_from"]  = c.validFrom;
            v["valid_until"] = c.validUntil;
            v["is_active"]   = c.isActive;
            v["created_at"]  = c.createdAt;
            v["created_by"]  = c.createdBy;
            // private_key_enc intentionally omitted
            return v;
        }

    } // anonymous namespace

    // -----------------------------------------------------------------------------
    // GET /api/management/certificates
    // -----------------------------------------------------------------------------
    drogon::Task<> CertificateController::getAll(
        drogon::HttpRequestPtr req,
        std::function<void(const drogon::HttpResponsePtr&)> callback)
    {
        if (!isCoreAdmin(req)) {
            callback(jsonError(drogon::k403Forbidden,
                "Only CORE_ADMIN can manage certificates"));
            co_return;
        }

        auto certs = co_await CertificateRepository::findAll();

        Json::Value body(Json::arrayValue);
        for (const auto& c : certs)
            body.append(certToJson(c));

        callback(jsonOk(body));
    }

    // -----------------------------------------------------------------------------
    // POST /api/management/certificates
    // Body: {
    //   "label":           "Peppol SMP Cert 2025",
    //   "cert_pem":        "-----BEGIN CERTIFICATE-----\n...",
    //   "private_key_pem": "-----BEGIN PRIVATE KEY-----\n..."
    // }
    // -----------------------------------------------------------------------------
    drogon::Task<> CertificateController::upload(
        drogon::HttpRequestPtr req,
        std::function<void(const drogon::HttpResponsePtr&)> callback)
    {
        if (!isCoreAdmin(req)) {
            callback(jsonError(drogon::k403Forbidden,
                "Only CORE_ADMIN can upload certificates"));
            co_return;
        }

        const auto body = req->getJsonObject();
        if (!body) {
            callback(jsonError(drogon::k400BadRequest, "JSON body required"));
            co_return;
        }

        const auto requireField = [&](const char* field) -> std::string {
            if (!body->isMember(field) || (*body)[field].asString().empty())
                return "";
            return (*body)[field].asString();
        };

        const std::string label      = requireField("label");
        const std::string certPem    = requireField("cert_pem");
        const std::string privateKey = requireField("private_key_pem");

        if (label.empty()) {
            callback(jsonError(drogon::k400BadRequest,
                "Field 'label' is required"));
            co_return;
        }
        if (certPem.empty()) {
            callback(jsonError(drogon::k400BadRequest,
                "Field 'cert_pem' is required"));
            co_return;
        }
        if (privateKey.empty()) {
            callback(jsonError(drogon::k400BadRequest,
                "Field 'private_key_pem' is required"));
            co_return;
        }

        // Validate cert is parseable X.509
        auto fingerprintResult = CryptoUtils::certFingerprint(certPem);
        if (!fingerprintResult) {
            callback(jsonError(drogon::k400BadRequest,
                "Field 'cert_pem' is not a valid X.509 PEM certificate"));
            co_return;
        }

        // Validate cert and key are a matching keypair
        auto pairResult = CryptoUtils::verifyCertKeyPair(certPem, privateKey);
        if (!pairResult) {
            callback(jsonError(drogon::k400BadRequest,
                "Certificate and private key do not match: " +
                pairResult.error().message));
            co_return;
        }

        // Extract validity dates
        auto validityResult = CryptoUtils::certValidity(certPem);
        std::string validFrom, validUntil;
        if (validityResult) {
            validFrom  = validityResult->notBefore;
            validUntil = validityResult->notAfter;
        }

        // Encrypt private key with master key
        const auto masterKey = Environment::masterKeyBytes();
        auto encResult = CryptoUtils::aesEncrypt(privateKey, masterKey);
        if (!encResult) {
            callback(jsonError(drogon::k500InternalServerError,
                "Failed to encrypt private key: " + encResult.error().message));
            co_return;
        }

        const std::string newId = co_await CertificateRepository::create(
            label, certPem, *encResult,
            *fingerprintResult, validFrom, validUntil,
            authUserId(req)
        );

        if (newId.empty()) {
            callback(jsonError(drogon::k500InternalServerError,
                "Failed to store certificate"));
            co_return;
        }

        auto created = co_await CertificateRepository::findById(newId);
        if (!created) {
            callback(jsonError(drogon::k500InternalServerError,
                "Certificate stored but not found"));
            co_return;
        }

        callback(jsonCreated(certToJson(*created)));
    }

    // -----------------------------------------------------------------------------
    // PATCH /api/management/certificates/:id/activate
    // -----------------------------------------------------------------------------
    drogon::Task<> CertificateController::activate(
        drogon::HttpRequestPtr req,
        std::function<void(const drogon::HttpResponsePtr&)> callback,
        std::string id)
    {
        if (!isCoreAdmin(req)) {
            callback(jsonError(drogon::k403Forbidden,
                "Only CORE_ADMIN can activate certificates"));
            co_return;
        }

        auto cert = co_await CertificateRepository::findById(id);
        if (!cert) {
            callback(jsonError(drogon::k404NotFound, "Certificate not found"));
            co_return;
        }

        if (cert->isActive) {
            callback(jsonError(drogon::k409Conflict,
                "Certificate is already active"));
            co_return;
        }

        if (!co_await CertificateRepository::activate(id)) {
            callback(jsonError(drogon::k500InternalServerError,
                "Failed to activate certificate"));
            co_return;
        }

        auto updated = co_await CertificateRepository::findById(id);
        callback(jsonOk(certToJson(*updated)));
    }

    // -----------------------------------------------------------------------------
    // DELETE /api/management/certificates/:id
    // Cannot delete the active certificate
    // -----------------------------------------------------------------------------
    drogon::Task<> CertificateController::remove(
        drogon::HttpRequestPtr req,
        std::function<void(const drogon::HttpResponsePtr&)> callback,
        std::string id)
    {
        if (!isCoreAdmin(req)) {
            callback(jsonError(drogon::k403Forbidden,
                "Only CORE_ADMIN can remove certificates"));
            co_return;
        }

        auto cert = co_await CertificateRepository::findById(id);
        if (!cert) {
            callback(jsonError(drogon::k404NotFound, "Certificate not found"));
            co_return;
        }

        if (cert->isActive) {
            callback(jsonError(drogon::k400BadRequest,
                "Cannot delete the active certificate. "
                "Activate another certificate first."));
            co_return;
        }

        if (!co_await CertificateRepository::remove(id)) {
            callback(jsonError(drogon::k500InternalServerError,
                "Failed to remove certificate"));
            co_return;
        }

        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::k204NoContent);
        callback(resp);
    }

} // namespace SMPPlusPlus
