//
// Created by Saadat Baig on 07.07.26.
//
#pragma once

#include <drogon/HttpController.h>


namespace SMPPlusPlus
{
    /**
     * EndpointAuditController - Flat cross-participant endpoint audit view.
     *
     * GET /api/management/tenants/:tenantId/endpoints
     *   Returns a paginated flat list of every endpoint under the given tenant,
     *   denormalized with its participant, document type, and process context.
     *
     * Intended for the admin UI "Endpoint List" audit view - the equivalent of
     * phoss-smp's flat endpoint list, showing everything at once with search,
     * sort, and pagination.
     *
     * The response is intentionally thin. To see the full endpoint detail
     * (PEM, descriptions, contact URLs), use the existing detail endpoint:
     *   GET /api/management/tenants/:tenantId/participants/:participantId
     *       /services/:serviceId
     */
    class EndpointAuditController
        : public drogon::HttpController<EndpointAuditController> {
    public:
        METHOD_LIST_BEGIN
            ADD_METHOD_TO(EndpointAuditController::listEndpoints,
                "/api/management/tenants/{tenantId}/endpoints",
                drogon::Get,
                "SMPPlusPlus::SessionOrBasicAuthFilter");
        METHOD_LIST_END

        drogon::Task<> listEndpoints(
            drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback,
            std::string tenantId
        );
    };

} // namespace SMPPlusPlus
