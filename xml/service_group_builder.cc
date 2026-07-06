//
// Created by Saadat Baig on 29.06.26.
//
#include "service_group_builder.h"
#include "xml_namespaces.h"
#include "utils/url_utils.h"

#include <sstream>


namespace SMPPlusPlus
{
    ServiceGroupBuilder::ServiceGroupBuilder(std::string smpBaseUrl)
        : smpBaseUrl_(std::move(smpBaseUrl))
    {}

    std::string ServiceGroupBuilder::build(const Participant& participant, const std::vector<ServiceMetadata>& services) const
    {
        pugi::xml_document doc;

        // XML declaration
        auto decl = doc.prepend_child(pugi::node_declaration);
        decl.append_attribute("version")  = "1.0";
        decl.append_attribute("encoding") = "UTF-8";

        // <ServiceGroup>
        auto sg = doc.append_child("smp:ServiceGroup");
        sg.append_attribute("xmlns:ds") = XmlNs::DSIG;
        sg.append_attribute("xmlns:id") = XmlNs::ID;
        sg.append_attribute("xmlns:smp") = XmlNs::SMP;
        sg.append_attribute("xmlns:wsa") = XmlNs::WSA;

        // <ParticipantIdentifier>
        auto pi = sg.append_child("id:ParticipantIdentifier");
        pi.append_attribute("scheme") = participant.identifierScheme.c_str();
        pi.text().set(participant.identifierValue.c_str());

        // <ServiceMetadataReferenceCollection>
        auto refs = sg.append_child("smp:ServiceMetadataReferenceCollection");

        const std::string encodedParticipant = UrlUtils::encodePeppolId(
            participant.identifierScheme,
            participant.identifierValue
        );

        for (const auto& svc : services) {
            const std::string encodedDocType = UrlUtils::encodePeppolId(
                svc.documentTypeScheme,
                svc.documentTypeValue
            );

            const std::string href =
                smpBaseUrl_ + "/" +
                encodedParticipant + "/services/" +
                encodedDocType;

            auto ref = refs.append_child("smp:ServiceMetadataReference");
            ref.append_attribute("href") = href.c_str();
        }

        // <Extension>
        addExtensionNode(sg, services.capacity());

        // Serialize
        std::ostringstream oss;
        doc.save(oss, "  ", pugi::format_default, pugi::encoding_utf8);
        return oss.str();
    }

    void ServiceGroupBuilder::addExtensionNode(pugi::xml_node& node, size_t serviceCount) const
    {
        auto ex = node.append_child("smp:Extension");
        ex.append_attribute("xmlns:smppp") = XmlNs::SMP_PLUS_PLUS;

        auto exSummary = SMPPlusPlus::Pugi::appendNamespacedChildFast(ex, "Summary", false);
        auto summaryTotal = SMPPlusPlus::Pugi::appendNamespacedChildFast(exSummary, "Entries", false);
        summaryTotal.text().set(serviceCount);
    }

} // namespace SMPPlusPlus
