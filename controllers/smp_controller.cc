//
// Created by Saadat Baig on 29.06.26.
//
#include "smp_controller.h"
#include "db/participant_repository.h"
#include "db/signing_key_cache.h"
#include "xml/service_group_builder.h"
#include "xml/service_metadata_builder.h"
#include "xml/xml_dsig_signer.h"
#include "utils/url_utils.h"
#include "utils/environment.h"

#include <drogon/drogon.h>


namespace SMPPlusPlus
{
    namespace {

        drogon::HttpResponsePtr xmlResponse(const std::string& xml) {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k200OK);
            resp->setContentTypeString("text/xml; charset=utf-8");
            resp->setBody(xml);
            return resp;
        }

        drogon::HttpResponsePtr notFound(const std::string& message) {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k404NotFound);
            resp->setContentTypeString("text/plain");
            resp->setBody(message);
            return resp;
        }

        drogon::HttpResponsePtr serviceUnavailable(const std::string& message) {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k503ServiceUnavailable);
            resp->setContentTypeString("text/plain");
            resp->setBody(message);
            return resp;
        }

        drogon::HttpResponsePtr badRequest(const std::string& message) {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k400BadRequest);
            resp->setContentTypeString("text/plain");
            resp->setBody(message);
            return resp;
        }

        // Find participant across all tenants by identifier scheme + value
        // The SMP protocol has no concept of tenants - we search globally
        drogon::Task<std::optional<Participant>> findParticipantGlobally(
            const std::string& scheme,
            const std::string& value)
        {
            auto db = drogon::app().getFastDbClient("default");

            try {
                auto rows = co_await db->execSqlCoro(
                    "SELECT id, tenant_id, identifier_scheme, identifier_value, "
                    "created_at, created_by FROM participants "
                    "WHERE identifier_scheme = $1 AND identifier_value = $2 LIMIT 1",
                    scheme, value
                );
                if (rows.empty()) co_return std::nullopt;

                Participant p;
                p.id                = rows[0]["id"].as<std::string>();
                p.tenantId          = rows[0]["tenant_id"].as<std::string>();
                p.identifierScheme  = rows[0]["identifier_scheme"].as<std::string>();
                p.identifierValue   = rows[0]["identifier_value"].as<std::string>();
                p.createdAt         = rows[0]["created_at"].as<std::string>();
                co_return p;
            } catch (const drogon::orm::DrogonDbException& e) {
                LOG_ERROR << "[SmpController] findParticipantGlobally: " << e.base().what();
                co_return std::nullopt;
            }
        }

    } // anonymous namespace

    // -----------------------------------------------------------------------------
    // GET /{participantId}
    // Returns ServiceGroup XML - no signing required
    // -----------------------------------------------------------------------------
    drogon::Task<> SmpController::getServiceGroup(
        drogon::HttpRequestPtr req,
        std::function<void(const drogon::HttpResponsePtr&)> callback,
        std::string participantId)
    {
        // Parse and decode the participant identifier
        auto peppolId = UrlUtils::parsePeppolId(participantId);
        if (!peppolId) {
            callback(badRequest(
                "Invalid participant identifier format. "
                "Expected: scheme%3A%3Avalue (percent-encoded)"));
            co_return;
        }

        // Look up participant globally across all tenants
        auto participant = co_await findParticipantGlobally(
            peppolId->scheme, peppolId->value);

        if (!participant) {
            callback(notFound(
                "Participant not found: " +
                peppolId->scheme + "::" + peppolId->value));
            co_return;
        }

        // Fetch all service metadata for this participant
        auto services = co_await ParticipantRepository::findServicesByParticipantId(
            participant->id, participant->tenantId);

        // Build ServiceGroup XML - instantiated per-request, isolated
        ServiceGroupBuilder builder(Environment::publicUrl());
        const std::string xml = builder.build(*participant, services);

        callback(xmlResponse(xml));
    }

    // -----------------------------------------------------------------------------
    // GET /{participantId}/services/{documentTypeId}
    // Returns SignedServiceMetadata XML - RSA-SHA256 signed
    // -----------------------------------------------------------------------------
    drogon::Task<> SmpController::getSignedServiceMetadata(
        drogon::HttpRequestPtr req,
        std::function<void(const drogon::HttpResponsePtr&)> callback,
        std::string participantId,
        std::string documentTypeId)
    {
        // Check signing key is available before doing any DB work
        auto keyEntry = SigningKeyCache::instance().get();
        if (!keyEntry) {
            callback(serviceUnavailable(
                "No active SMP signing certificate configured. "
                "Contact the SMP operator."));
            co_return;
        }

        // Parse participant identifier
        auto peppolId = UrlUtils::parsePeppolId(participantId);
        if (!peppolId) {
            callback(badRequest("Invalid participant identifier format"));
            co_return;
        }

        // Parse document type identifier
        auto docTypeId = UrlUtils::parsePeppolId(documentTypeId);
        if (!docTypeId) {
            callback(badRequest("Invalid document type identifier format"));
            co_return;
        }

        // Look up participant globally
        auto participant = co_await findParticipantGlobally(
            peppolId->scheme, peppolId->value);

        if (!participant) {
            callback(notFound(
                "Participant not found: " +
                peppolId->scheme + "::" + peppolId->value));
            co_return;
        }

        // Find the specific service metadata
        auto db = drogon::app().getFastDbClient("default");
        std::optional<ServiceMetadata> targetService;

        try {
            auto rows = co_await db->execSqlCoro(
                "SELECT id FROM service_metadata "
                "WHERE participant_id = $1 "
                "AND document_type_scheme = $2 "
                "AND document_type_value = $3 LIMIT 1",
                participant->id,
                docTypeId->scheme,
                docTypeId->value
            );

            if (rows.empty()) {
                callback(notFound(
                    "Service metadata not found for document type: " +
                    docTypeId->scheme + "::" + docTypeId->value));
                co_return;
            }

            const std::string serviceId = rows[0]["id"].as<std::string>();
            targetService = co_await ParticipantRepository::findServiceById(
                serviceId, participant->id);

        } catch (const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "[SmpController] service lookup failed: " << e.base().what();
            callback(serviceUnavailable("Internal error during service lookup"));
            co_return;
        }

        if (!targetService) {
            callback(notFound("Service metadata not found"));
            co_return;
        }

        // Build ServiceMetadata XML - per-request instance
        ServiceMetadataBuilder smBuilder;
        pugi::xml_document smDoc = smBuilder.build(*participant, *targetService);

        // Sign - per-request, uses cached EVP_PKEY* via shared_ptr
        auto signResult = XmlDsigSigner::sign(
            smDoc,
            keyEntry->key.get(),
            keyEntry->certPem
        );

        if (!signResult) {
            LOG_ERROR << "[SmpController] Signing failed: "
                      << signResult.error().message;
            callback(serviceUnavailable("Failed to sign service metadata"));
            co_return;
        }

        callback(xmlResponse(*signResult));
    }

} // namespace SMPPlusPlus
