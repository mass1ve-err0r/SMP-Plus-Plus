//
// Created by Saadat Baig on 29.06.26.
//
#include "service_metadata_builder.h"
#include "xml_namespaces.h"

#include <sstream>
#include <algorithm>


namespace SMPPlusPlus
{
    pugi::xml_document
    ServiceMetadataBuilder::build(const Participant& participant, const ServiceMetadata& service) const
    {
        pugi::xml_document doc;

        auto decl = doc.prepend_child(pugi::node_declaration);
        decl.append_attribute("version")  = "1.0";
        decl.append_attribute("encoding") = "UTF-8";

        // <ServiceMetadata> - root element before signing wraps it
        auto sm = doc.append_child("smp:ServiceMetadata");

        // <ServiceInformation>
        auto si = sm.append_child("smp:ServiceInformation");

        // <ParticipantIdentifier>
        auto pi = si.append_child("id:ParticipantIdentifier");
        pi.append_attribute("scheme") = participant.identifierScheme.c_str();
        pi.text().set(participant.identifierValue.c_str());

        // <DocumentIdentifier>
        auto di = si.append_child("id:DocumentIdentifier");
        di.append_attribute("scheme") = service.documentTypeScheme.c_str();
        di.text().set(service.documentTypeValue.c_str());

        // <ProcessList>
        auto processList = si.append_child("smp:ProcessList");

        for (const auto& process : service.processes)
        {
            auto proc = processList.append_child("smp:Process");

            // <ProcessIdentifier>
            auto procId = proc.append_child("id:ProcessIdentifier");
            procId.append_attribute("scheme") = process.processScheme.c_str();
            procId.text().set(process.processValue.c_str());

            // <ServiceEndpointList>
            auto endpointList = proc.append_child("smp:ServiceEndpointList");

            for (const auto& endpoint : process.endpoints)
            {
                appendEndpoint(endpointList, endpoint);
            }
        }

        return doc;
    }

    void
    ServiceMetadataBuilder::appendEndpoint(pugi::xml_node& endpointList, const Endpoint& endpoint) const
    {
        auto ep = endpointList.append_child("smp:Endpoint");
        ep.append_attribute("transportProfile") = endpoint.transportProfile.c_str();

        // <EndpointReference>
        auto er = ep.append_child("wsa:EndpointReference");
        er.append_child("wsa:Address").text().set(endpoint.endpointUrl.c_str());

        // <RequireBusinessLevelSignature>
        ep.append_child("smp:RequireBusinessLevelSignature")
          .text().set(endpoint.requireBls ? "true" : "false");

        // <Certificate> - raw base64, no PEM headers
        ep.append_child("smp:Certificate")
          .text().set(stripPemHeaders(endpoint.apCertificatePem).c_str());

        // <ServiceDescription> (optional)
        if (!endpoint.serviceDescription.empty())
        {
            ep.append_child("smp:ServiceDescription")
              .text().set(endpoint.serviceDescription.c_str());
        }

        // <TechnicalContactUrl> (optional)
        if (!endpoint.technicalContactUrl.empty())
        {
            ep.append_child("smp:TechnicalContactUrl")
              .text().set(endpoint.technicalContactUrl.c_str());
        }
    }

    std::string
    ServiceMetadataBuilder::stripPemHeaders(const std::string& pem)
    {
        std::string result;
        result.reserve(pem.size());

        std::istringstream iss(pem);
        std::string line;
        while (std::getline(iss, line))
        {
            // Skip -----BEGIN...----- and -----END...----- lines
            if (line.find("-----") != std::string::npos) continue;

            // Strip trailing whitespace/CR
            while (!line.empty() &&
                   (line.back() == '\r' || line.back() == '\n' ||
                    line.back() == ' '))
            {
                line.pop_back();
            }
            result += line;
        }

        return result;
    }

} // namespace SMPPlusPlus
