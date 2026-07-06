//
// Created by Saadat Baig on 29.06.26.
//
#include "sml_registration_controller.h"
#include "db/sml_repository.h"
#include "db/certificate_repository.h"
#include "db/settings_repository.h"
#include "db/sml_registration_service.h"
#include "utils/crypto_utils.h"
#include "utils/environment.h"

#include <drogon/drogon.h>
#include <thread>


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

        struct PreflightResult {
            std::string smpId;
            std::string smlEndpoint;
            std::string certPem;
            std::string privateKeyPem;
        };

        using PreflightError   = std::pair<drogon::HttpStatusCode, std::string>;
        using PreflightOutcome = tl::expected<PreflightResult, PreflightError>;

        drogon::Task<PreflightOutcome> preflight(const std::string& smlId) {
            auto sml = co_await SmlRepository::findById(smlId);
            if (!sml)
                co_return tl::unexpected(PreflightError{
                    drogon::k404NotFound, "SML configuration not found"});

            if (!sml->isEnabled)
                co_return tl::unexpected(PreflightError{
                    drogon::k400BadRequest,
                    "SML configuration is disabled. Enable it first."});

            if (sml->managementEndpoint.empty())
                co_return tl::unexpected(PreflightError{
                    drogon::k400BadRequest,
                    "SML management endpoint is not configured."});

            auto smpIdSetting = co_await SettingsRepository::findByKey("smp.id");
            if (!smpIdSetting || smpIdSetting->value.empty())
                co_return tl::unexpected(PreflightError{
                    drogon::k400BadRequest,
                    "smp.id is not configured. Set it in Settings before registering."});

            auto cert = co_await CertificateRepository::findActive();
            if (!cert)
                co_return tl::unexpected(PreflightError{
                    drogon::k400BadRequest,
                    "No active SMP certificate. Upload and activate a certificate first."});

            const auto masterKey = Environment::masterKeyBytes();
            auto decryptResult   = CryptoUtils::aesDecrypt(cert->privateKeyEnc, masterKey);
            if (!decryptResult)
                co_return tl::unexpected(PreflightError{
                    drogon::k500InternalServerError,
                    "Failed to decrypt SMP private key: " +
                    decryptResult.error().message});

            co_return PreflightResult{
                smpIdSetting->value,
                sml->managementEndpoint,
                cert->certPem,
                *decryptResult
            };
        }

    } // anonymous namespace

    // -----------------------------------------------------------------------------
    // POST /api/management/sml/:id/register
    // -----------------------------------------------------------------------------
    drogon::Task<> SmlRegistrationController::registerSmp(
        drogon::HttpRequestPtr req,
        std::function<void(const drogon::HttpResponsePtr&)> callback,
        std::string id)
    {
        // Async DB preflight on the IO thread via coroutine
        auto pre = co_await preflight(id);
        if (!pre) {
            callback(jsonError(pre.error().first, pre.error().second));
            co_return;
        }

        // Blocking SOAP call offloaded to a detached thread.
        // Drogon's response callback is thread-safe to invoke from any thread.
        std::thread([p = std::move(*pre), callback]() mutable {
            auto result = SmlRegistrationService::registerSmp(
                p.smlEndpoint, p.smpId, p.certPem, p.privateKeyPem);

            if (!result) {
                callback(jsonError(drogon::k502BadGateway,
                    "SML registration failed: " + result.error().message));
                return;
            }

            Json::Value body;
            body["message"] = "SMP successfully registered with SML";
            callback(jsonOk(body));
        }).detach();
    }

    // -----------------------------------------------------------------------------
    // POST /api/management/sml/:id/deregister
    // -----------------------------------------------------------------------------
    drogon::Task<> SmlRegistrationController::deregisterSmp(
        drogon::HttpRequestPtr req,
        std::function<void(const drogon::HttpResponsePtr&)> callback,
        std::string id)
    {
        auto pre = co_await preflight(id);
        if (!pre) {
            callback(jsonError(pre.error().first, pre.error().second));
            co_return;
        }

        std::thread([p = std::move(*pre), callback]() mutable {
            auto result = SmlRegistrationService::deregisterSmp(
                p.smlEndpoint, p.smpId, p.certPem, p.privateKeyPem);

            if (!result) {
                callback(jsonError(drogon::k502BadGateway,
                    "SML deregistration failed: " + result.error().message));
                return;
            }

            Json::Value body;
            body["message"] = "SMP successfully deregistered from SML";
            callback(jsonOk(body));
        }).detach();
    }

} // namespace SMPPlusPlus
