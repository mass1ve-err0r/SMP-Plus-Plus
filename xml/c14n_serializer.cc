//
// Created by Saadat Baig on 29.06.26.
//
#include "c14n_serializer.h"

#include <vector>
#include <algorithm>


namespace SMPPlusPlus
{
    std::string C14nSerializer::serialize(const pugi::xml_document& doc) {
        std::string out;
        out.reserve(4096);
        // Skip the XML declaration node - C14N output has none
        for (auto child = doc.first_child(); child; child = child.next_sibling()) {
            if (child.type() == pugi::node_declaration) continue;
            serializeNode(child, out);
        }
        return out;
    }

    std::string C14nSerializer::serialize(const pugi::xml_node& node) {
        std::string out;
        out.reserve(4096);
        serializeNode(node, out);
        return out;
    }

    void C14nSerializer::serializeNode(
        const pugi::xml_node& node,
        std::string& out)
    {
        switch (node.type()) {
            case pugi::node_element:
                serializeElement(node, out);
                break;
            case pugi::node_pcdata:
            case pugi::node_cdata:
                escapeText(node.value(), out);
                break;
            case pugi::node_comment:
                // Comments included in C14N but not needed for SMP signing
                break;
            default:
                break;
        }
    }

    void C14nSerializer::serializeElement(
        const pugi::xml_node& node,
        std::string& out)
    {
        out += '<';
        out += node.name();

        // Collect and sort namespace declarations and regular attributes
        // C14N ordering:
        //   1. Namespace declarations sorted by prefix (xmlns before xmlns:*)
        //   2. Regular attributes sorted by name

        struct Attr {
            std::string name;
            std::string value;
            bool isNs;
        };

        std::vector<Attr> nsAttrs;
        std::vector<Attr> regAttrs;

        for (auto attr = node.first_attribute(); attr; attr = attr.next_attribute()) {
            std::string name  = attr.name();
            std::string value = attr.value();

            if (name == "xmlns" || name.substr(0, 6) == "xmlns:") {
                nsAttrs.push_back({std::move(name), std::move(value), true});
            } else {
                regAttrs.push_back({std::move(name), std::move(value), false});
            }
        }

        // Sort namespace declarations: "xmlns" first, then "xmlns:*" alphabetically
        std::sort(nsAttrs.begin(), nsAttrs.end(),
            [](const Attr& a, const Attr& b) {
                // "xmlns" < "xmlns:anything"
                if (a.name == "xmlns") return true;
                if (b.name == "xmlns") return false;
                return a.name < b.name;
            }
        );

        // Sort regular attributes alphabetically
        std::sort(regAttrs.begin(), regAttrs.end(),
            [](const Attr& a, const Attr& b) {
                return a.name < b.name;
            }
        );

        // Output namespace declarations
        for (const auto& attr : nsAttrs) {
            out += ' ';
            out += attr.name;
            out += "=\"";
            escapeAttr(attr.value, out);
            out += '"';
        }

        // Output regular attributes
        for (const auto& attr : regAttrs) {
            out += ' ';
            out += attr.name;
            out += "=\"";
            escapeAttr(attr.value, out);
            out += '"';
        }

        out += '>';

        // Serialize children
        for (auto child = node.first_child(); child; child = child.next_sibling()) {
            serializeNode(child, out);
        }

        // Always use explicit closing tag (no self-closing in C14N)
        out += "</";
        out += node.name();
        out += '>';
    }

    void C14nSerializer::escapeText(std::string_view text, std::string& out) {
        for (char c : text) {
            switch (c) {
                case '&':  out += "&amp;";  break;
                case '<':  out += "&lt;";   break;
                case '>':  out += "&gt;";   break;
                case '\r': out += "&#xD;";  break;
                default:   out += c;        break;
            }
        }
    }

    void C14nSerializer::escapeAttr(std::string_view text, std::string& out) {
        for (char c : text) {
            switch (c) {
                case '&':  out += "&amp;";  break;
                case '<':  out += "&lt;";   break;
                case '"':  out += "&quot;"; break;
                case '\t': out += "&#x9;";  break;
                case '\n': out += "&#xA;";  break;
                case '\r': out += "&#xD;";  break;
                default:   out += c;        break;
            }
        }
    }

} // namespace SMPPlusPlus
