//
// Created by Saadat Baig on 29.06.26.
//
#pragma once

#include <drogon/HttpController.h>


namespace SMPPlusPlus
{
    /**
     * SmlRegistrationController - Register/deregister this SMP with an SML.
     *
     * These are one-time actions triggered from the Settings UI.
     * Both CORE_ADMIN and MANAGEMENT users can trigger registration.
     *
     * POST /api/management/sml/:id/register    → register with this SML
     * POST /api/management/sml/:id/deregister  → deregister from this SML
     *
     * Prerequisites:
     *   - smp.id must be set in system_settings
     *   - An active SMP certificate must exist
     *   - The SML configuration must be enabled
     */
    class SmlRegistrationController
        : public drogon::HttpController<SmlRegistrationController> {
    public:
        METHOD_LIST_BEGIN
            ADD_METHOD_TO(SmlRegistrationController::registerSmp,
                "/api/management/sml/{id}/register",
                drogon::Post, "SMPPlusPlus::BasicAuthFilter");

            ADD_METHOD_TO(SmlRegistrationController::deregisterSmp,
                "/api/management/sml/{id}/deregister",
                drogon::Post, "SMPPlusPlus::BasicAuthFilter");
        METHOD_LIST_END

        drogon::Task<> registerSmp(drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback,
            std::string id);

        drogon::Task<> deregisterSmp(drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback,
            std::string id);
    };

} // namespace SMPPlusPlus
