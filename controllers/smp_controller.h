//
// Created by Saadat Baig on 29.06.26.
//
#pragma once

#include <drogon/HttpController.h>


namespace SMPPlusPlus
{
    /**
     * SmpController - Public Peppol SMP protocol endpoints.
     *
     * These are the outward-facing endpoints that implement the SMP spec.
     * No authentication required - they are publicly accessible per the spec.
     *
     * GET /{participantId}
     *   Returns ServiceGroup XML listing all service metadata references.
     *   participantId is percent-encoded: "scheme%3A%3Avalue"
     *
     * GET /{participantId}/services/{documentTypeId}
     *   Returns SignedServiceMetadata XML, RSA-SHA256 signed with the
     *   active SMP certificate.
     *   documentTypeId is percent-encoded: "scheme%3A%3Avalue"
     */
    class SmpController : public drogon::HttpController<SmpController> {
    public:
        METHOD_LIST_BEGIN
            ADD_METHOD_TO(SmpController::getServiceGroup,
                "/{participantId}",
                drogon::Get);

            ADD_METHOD_TO(SmpController::getSignedServiceMetadata,
                "/{participantId}/services/{documentTypeId}",
                drogon::Get);
        METHOD_LIST_END

        drogon::Task<> getServiceGroup(
            drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback,
            std::string participantId
        );

        drogon::Task<> getSignedServiceMetadata(
            drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback,
            std::string participantId,
            std::string documentTypeId
        );
    };

} // namespace SMPPlusPlus
