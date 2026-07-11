//
// Created by Saadat Baig on 29.06.26.
//
#pragma once

#include <pugixml.hpp>

#include <string>

#include "db/participant_repository.h"


namespace SMPPlusPlus
{
    /**
     * ServiceMetadataBuilder - Builds the ServiceMetadata XML element.
     *
     * Instantiated per-request. Produces the inner ServiceMetadata node
     * that will be wrapped in SignedServiceMetadata by XmlDsigSigner.
     *
     * The document is built already in canonical form (namespace declarations
     * and attributes sorted) so that C14N serialization is a deterministic
     * output with no reordering needed.
     */
    class ServiceMetadataBuilder
    {
    public:

        ServiceMetadataBuilder() = default;

        /**
         * Build a complete pugi::xml_document containing ServiceMetadata.
         * The document is structured ready for signing — the signer will
         * wrap it in SignedServiceMetadata and append ds:Signature.
         */
        pugi::xml_document
        build(const Participant& participant, const ServiceMetadata& service) const;

    private:

        void
        appendEndpoint(pugi::xml_node& endpointList, const Endpoint& endpoint) const;

        // Strips PEM headers/footers, returns raw base64 cert data
        // The SMP spec requires the raw base64 without -----BEGIN/END----- lines
        static std::string
        stripPemHeaders(const std::string& pem);
    };

} // namespace SMPPlusPlus
