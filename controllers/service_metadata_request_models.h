//
// Created by Saadat Baig on 06.07.26.
//
#pragma once

#include <string>
#include <vector>


namespace SMPPlusPlus
{
    class CompleteServiceInput {
    public:
        CompleteServiceInput()
        {
            processes.reserve(1);
        }

        class Endpoint
        {
        public:
            std::string transportProfile = "peppol-transport-as4-v2_0";
            std::string endpointUrl;
            std::string apCertificatePem;
            bool        requireBls = false;
            std::string serviceActivationDate;
            std::string serviceExpirationDate;
            std::string technicalContactUrl;
            std::string technicalInformationUrl;
            std::string serviceDescription;
        };

        class Process
        {
        public:
            Process() {
                // by default, the API endpoint using this class will provide us 1 endpoint per call so we can optimize
                // already here but keep it fundamentally dynamic.
                endpoints.reserve(1);
            }

            std::string processScheme = "cenbii-procid-ubl";
            std::string processValue;
            std::vector<Endpoint> endpoints;
        };

        std::string documentTypeScheme = "busdox-docid-qns";
        std::string documentTypeValue;
        std::vector<Process> processes;
    };

}
