//
// Created by Saadat Baig on 29.06.26.
//
#pragma once

#include <drogon/HttpController.h>


namespace SMPPlusPlus
{
    /**
     * CertificateController - SMP signing certificate management.
     *
     * Instance-level. CORE_ADMIN only.
     *
     * GET    /api/management/certificates           → list all
     * POST   /api/management/certificates           → upload new cert + private key
     * PATCH  /api/management/certificates/:id/activate → set as active signing cert
     * DELETE /api/management/certificates/:id       → remove (cannot remove active)
     */
    class CertificateController
        : public drogon::HttpController<CertificateController> {
    public:
        METHOD_LIST_BEGIN
            ADD_METHOD_TO(CertificateController::getAll,
                "/api/management/certificates",
                drogon::Get, "SMPPlusPlus::SessionOrBasicAuthFilter");

        ADD_METHOD_TO(CertificateController::upload,
            "/api/management/certificates",
            drogon::Post, "SMPPlusPlus::SessionOrBasicAuthFilter");

        ADD_METHOD_TO(CertificateController::activate,
            "/api/management/certificates/{id}/activate",
            drogon::Patch, "SMPPlusPlus::SessionOrBasicAuthFilter");

        ADD_METHOD_TO(CertificateController::remove,
            "/api/management/certificates/{id}",
            drogon::Delete, "SMPPlusPlus::SessionOrBasicAuthFilter");
        METHOD_LIST_END

        drogon::Task<> getAll(drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback);

        drogon::Task<> upload(drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback);

        drogon::Task<> activate(drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback,
            std::string id);

        drogon::Task<> remove(drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback,
            std::string id);
    };

} // namespace SMPPlusPlus
