//
// Created by Saadat Baig on 29.06.26.
//
#pragma once

#include <drogon/HttpController.h>


namespace SMPPlusPlus
{
    /**
     * ParticipantController - Participant management under a tenant.
     *
     * GET    /api/management/tenants/:tenantId/participants
     * GET    /api/management/tenants/:tenantId/participants/:id
     * POST   /api/management/tenants/:tenantId/participants
     * DELETE /api/management/tenants/:tenantId/participants/:id
     *
     * No PUT - a participant's identifier is immutable once registered.
     * To change an identifier, delete and re-create.
     */
    class ParticipantController : public drogon::HttpController<ParticipantController> {
    public:
        METHOD_LIST_BEGIN
            ADD_METHOD_TO(ParticipantController::getAll,
                "/api/management/tenants/{tenantId}/participants",
                drogon::Get,
                "SMPPlusPlus::SessionOrBasicAuthFilter");

            ADD_METHOD_TO(ParticipantController::getOne,
                "/api/management/tenants/{tenantId}/participants/{id}",
                drogon::Get,
                "SMPPlusPlus::SessionOrBasicAuthFilter");

            ADD_METHOD_TO(ParticipantController::create,
                "/api/management/tenants/{tenantId}/participants",
                drogon::Post,
                "SMPPlusPlus::SessionOrBasicAuthFilter");

            ADD_METHOD_TO(ParticipantController::remove,
                "/api/management/tenants/{tenantId}/participants/{id}",
                drogon::Delete,
                "SMPPlusPlus::SessionOrBasicAuthFilter");
        METHOD_LIST_END

        drogon::Task<> getAll(
            drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback,
            std::string tenantId
        );

        drogon::Task<> getOne(
            drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback,
            std::string tenantId,
            std::string id
        );

        drogon::Task<> create(
            drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback,
            std::string tenantId
        );

        drogon::Task<> remove(
            drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback,
            std::string tenantId,
            std::string id
        );
    };

} // namespace SMPPlusPlus
