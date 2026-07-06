//
// Created by Saadat Baig on 29.06.26.
//
#pragma once

#include <string>
#include <tl/expected.hpp>


namespace SMPPlusPlus
{
    /**
     * SmlRegistrationService - Registers/deregisters this SMP with an SML.
     *
     * The SML management API is a simple SOAP service. We hand-roll the
     * SOAP envelope via libcurl since the API surface is minimal:
     *   - createParticipantIdentifier (register SMP)
     *   - deleteParticipantIdentifier (deregister SMP)
     *
     * Authentication: mutual TLS using the active SMP certificate.
     *
     * All methods are blocking - call from a worker thread or before app.run().
     * For the management UI trigger, Drogon's execSyncTask or a detached
     * async call handles offloading.
     */
    class SmlRegistrationService {
    public:
        SmlRegistrationService() = delete;

        struct Error {
            std::string message;
            int httpStatus = 0;
        };

        template<typename T>
        using Result = tl::expected<T, Error>;

        /**
         * Register this SMP with the given SML endpoint.
         *
         * @param smlEndpoint     SOAP endpoint URL of the SML
         * @param smpId           The SMP's registered identifier
         * @param certPem         SMP signing certificate (public, PEM)
         * @param privateKeyPem   SMP signing private key (decrypted PEM)
         */
        static Result<void> registerSmp(
            const std::string& smlEndpoint,
            const std::string& smpId,
            const std::string& certPem,
            const std::string& privateKeyPem
        );

        /**
         * Deregister this SMP from the given SML endpoint.
         */
        static Result<void> deregisterSmp(
            const std::string& smlEndpoint,
            const std::string& smpId,
            const std::string& certPem,
            const std::string& privateKeyPem
        );

    private:
        static std::string buildRegisterEnvelope(const std::string& smpId);
        static std::string buildDeregisterEnvelope(const std::string& smpId);

        static Result<void> callSml(
            const std::string& smlEndpoint,
            const std::string& soapAction,
            const std::string& envelope,
            const std::string& certPem,
            const std::string& privateKeyPem
        );
    };

} // namespace SMPPlusPlus
