//
// Created by Saadat Baig on 29.06.26.
//
#pragma once

#include <string>
#include <vector>
#include <pugixml.hpp>
#include "db/participant_repository.h"


namespace SMPPlusPlus
{
    /**
     * ServiceGroupBuilder - Builds a Peppol SMP ServiceGroup XML document.
     *
     * Instantiated per-request. Not reusable across requests.
     *
     * Output conforms to:
     * http://busdox.org/serviceMetadata/publishing/1.0/
     *
     * Example output:
     *   <ServiceGroup xmlns="..." xmlns:id="...">
     *     <ParticipantIdentifier scheme="iso6523-actorid-upis">0088:123456789</ParticipantIdentifier>
     *     <ServiceMetadataReferenceCollection>
     *       <ServiceMetadataReference href="https://my.smp.com/..."/>
     *     </ServiceMetadataReferenceCollection>
     *   </ServiceGroup>
     */
    class ServiceGroupBuilder {
    public:
        explicit ServiceGroupBuilder(std::string smpBaseUrl);

        /**
         * Build the ServiceGroup document.
         *
         * @param participant  The participant whose ServiceGroup is being served
         * @param services     All service metadata entries for this participant
         * @return             XML string ready to send as response body
         */
        std::string build(
            const Participant& participant,
            const std::vector<ServiceMetadata>& services
        ) const;

    private:
        std::string smpBaseUrl_;
    };

} // namespace SMPPlusPlus
