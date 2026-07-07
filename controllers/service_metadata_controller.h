//
// Created by Saadat Baig on 29.06.26.
//
#pragma once

#include <drogon/HttpController.h>


namespace SMPPlusPlus
{
    /**
     * ServiceMetadataController - Service metadata, processes and endpoints.
     *
     * Service Metadata (document types per participant):
     * GET    /api/management/tenants/:tenantId/participants/:participantId/services
     * GET    /api/management/tenants/:tenantId/participants/:participantId/services/:serviceId
     * POST   /api/management/tenants/:tenantId/participants/:participantId/services
     * DELETE /api/management/tenants/:tenantId/participants/:participantId/services/:serviceId
     *
     * Processes (under a service metadata):
     * POST   /api/management/tenants/:tenantId/participants/:participantId/services/:serviceId/processes
     * DELETE /api/management/tenants/:tenantId/participants/:participantId/services/:serviceId/processes/:processId
     *
     * Endpoints (under a process):
     * POST   /api/management/tenants/:tenantId/participants/:participantId/services/:serviceId/processes/:processId/endpoints
     * DELETE /api/management/tenants/:tenantId/participants/:participantId/services/:serviceId/processes/:processId/endpoints/:endpointId
     */
    class ServiceMetadataController : public drogon::HttpController<ServiceMetadataController> {
    public:
        METHOD_LIST_BEGIN
            // Service Metadata
            ADD_METHOD_TO(ServiceMetadataController::getServices,
                "/api/management/tenants/{tenantId}/participants/{participantId}/services",
                drogon::Get, "SMPPlusPlus::BasicAuthFilter");

            ADD_METHOD_TO(ServiceMetadataController::getService,
                "/api/management/tenants/{tenantId}/participants/{participantId}/services/{serviceId}",
                drogon::Get, "SMPPlusPlus::BasicAuthFilter");

            ADD_METHOD_TO(ServiceMetadataController::createService,
                "/api/management/tenants/{tenantId}/participants/{participantId}/services",
                drogon::Post, "SMPPlusPlus::BasicAuthFilter");

            ADD_METHOD_TO(ServiceMetadataController::removeService,
                "/api/management/tenants/{tenantId}/participants/{participantId}/services/{serviceId}",
                drogon::Delete, "SMPPlusPlus::BasicAuthFilter");

            // Processes
            ADD_METHOD_TO(ServiceMetadataController::createProcess,
                "/api/management/tenants/{tenantId}/participants/{participantId}/services/{serviceId}/processes",
                drogon::Post, "SMPPlusPlus::BasicAuthFilter");

            ADD_METHOD_TO(ServiceMetadataController::removeProcess,
                "/api/management/tenants/{tenantId}/participants/{participantId}/services/{serviceId}/processes/{processId}",
                drogon::Delete, "SMPPlusPlus::BasicAuthFilter");

            // One-shot complete service creation (inline)
            ADD_METHOD_TO(ServiceMetadataController::createCompleteInline,
                "/api/management/tenants/{tenantId}/participants/{participantId}/services/completeInline",
                drogon::Post, "SMPPlusPlus::BasicAuthFilter");

            // One-shot complete service creation
            ADD_METHOD_TO(ServiceMetadataController::createComplete,
                "/api/management/tenants/{tenantId}/participants/{participantId}/services/complete",
                drogon::Post, "SMPPlusPlus::BasicAuthFilter");

            // Endpoints
            ADD_METHOD_TO(ServiceMetadataController::createEndpoint,
                "/api/management/tenants/{tenantId}/participants/{participantId}/services/{serviceId}/processes/{processId}/endpoints",
                drogon::Post, "SMPPlusPlus::BasicAuthFilter");

            ADD_METHOD_TO(ServiceMetadataController::removeEndpoint,
                "/api/management/tenants/{tenantId}/participants/{participantId}/services/{serviceId}/processes/{processId}/endpoints/{endpointId}",
                drogon::Delete, "SMPPlusPlus::BasicAuthFilter");
        METHOD_LIST_END

        // Service Metadata
        drogon::Task<> getServices(drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback,
            std::string tenantId, std::string participantId);

        drogon::Task<> getService(drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback,
            std::string tenantId, std::string participantId, std::string serviceId);

        drogon::Task<> createService(drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback,
            std::string tenantId, std::string participantId);

        drogon::Task<> removeService(drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback,
            std::string tenantId, std::string participantId, std::string serviceId);

        drogon::Task<> createCompleteInline(drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback,
            std::string tenantId, std::string participantId);

        drogon::Task<> createComplete(drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback,
            std::string tenantId, std::string participantId);

        // Processes
        drogon::Task<> createProcess(drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback,
            std::string tenantId, std::string participantId, std::string serviceId);

        drogon::Task<> removeProcess(drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback,
            std::string tenantId, std::string participantId,
            std::string serviceId, std::string processId);

        // Endpoints
        drogon::Task<> createEndpoint(drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback,
            std::string tenantId, std::string participantId,
            std::string serviceId, std::string processId);

        drogon::Task<> removeEndpoint(drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback,
            std::string tenantId, std::string participantId,
            std::string serviceId, std::string processId, std::string endpointId);
    };

} // namespace SMPPlusPlus
