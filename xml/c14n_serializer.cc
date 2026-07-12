//
// Created by Saadat Baig on 29.06.26.
//
#include "c14n_serializer.h"

#include <vector>
#include <algorithm>


namespace SMPPlusPlus
{
    namespace
    {
        constexpr std::string_view XML_NS = "http://www.w3.org/XML/1998/namespace";

        bool
        isNamespaceDeclaration(std::string_view name)
        {
            return name == "xmlns" || name.rfind("xmlns:", 0) == 0;
        }

        std::string
        namespaceDeclarationPrefix(std::string_view name)
        {
            if (name == "xmlns") return {};
            return std::string{name.substr(6)};
        }

        std::string
        qnamePrefix(std::string_view qname)
        {
            const auto pos = qname.find(':');
            if (pos == std::string_view::npos) return {};

            return std::string{qname.substr(0, pos)};
        }

        std::string
        qnameLocalName(std::string_view qname)
        {
            const auto pos = qname.find(':');
            if (pos == std::string_view::npos) return std::string{qname};

            return std::string{qname.substr(pos + 1)};
        }

        std::string
        namespaceUriForPrefix(const std::string& prefix, const C14nSerializer::NamespaceMap& namespaces)
        {
            if (prefix == "xml") return std::string{XML_NS};

            const auto it = namespaces.find(prefix);
            if (it == namespaces.end()) return {};

            return it->second;
        }

        bool
        shouldRenderNamespace(const std::string& prefix, const std::string& uri, const C14nSerializer::NamespaceMap& renderedNamespaces)
        {
            if (prefix == "xml" && uri == XML_NS) return false;

            const auto it = renderedNamespaces.find(prefix);
            if (it == renderedNamespaces.end())
            {
                return !uri.empty() || !prefix.empty();
            }

            return it->second != uri;
        }

        void
        appendNamespaceDeclaration(std::string& out, const std::string& prefix, const std::string& uri)
        {
            out += ' ';
            out += "xmlns";

            if (!prefix.empty())
            {
                out += ':';
                out += prefix;
            }

            out += "=\"";
            C14nSerializer::escapeAttr(uri, out);
            out += '"';
        }
    }

    std::string
    C14nSerializer::serialize(const pugi::xml_document& doc)
    {
        std::string out;
        out.reserve(4096);

        const NamespaceMap emptyNamespaces;
        for (auto child = doc.first_child(); child; child = child.next_sibling())
        {
            if (child.type() == pugi::node_declaration) continue;
            serializeNode(child, out, emptyNamespaces, emptyNamespaces);
        }

        return out;
    }

    std::string
    C14nSerializer::serialize(const pugi::xml_node& node)
    {
        std::string out;
        out.reserve(4096);

        const NamespaceMap ancestorNamespaces = collectAncestorNamespaces(node);
        const NamespaceMap emptyRenderedNamespaces;

        serializeNode(node, out, ancestorNamespaces, emptyRenderedNamespaces);

        return out;
    }

    void
    C14nSerializer::serializeNode(const pugi::xml_node& node, std::string& out, const NamespaceMap& inScopeNamespaces, const NamespaceMap& renderedNamespaces)
    {
        switch (node.type())
        {
            case pugi::node_document:
            {
                const NamespaceMap emptyRenderedNamespaces;

                for (auto child = node.first_child(); child; child = child.next_sibling())
                {
                    if (child.type() == pugi::node_declaration) continue;
                    serializeNode(child, out, inScopeNamespaces, emptyRenderedNamespaces);
                }

                break;
            }

            case pugi::node_element:
            {
                serializeElement(node, out, inScopeNamespaces, renderedNamespaces);
                break;
            }

            case pugi::node_pcdata:
            case pugi::node_cdata:
            {
                escapeText(node.value(), out);
                break;
            }

            // we can ignore clang-tidy here in favor of LLVM ding the lifting for us.
            case pugi::node_comment:
            {
                break;
            }

            default:
            {
                break;
            }
        }
    }

    void
    C14nSerializer::serializeElement(const pugi::xml_node& node, std::string& out, const NamespaceMap& inScopeNamespaces, const NamespaceMap& renderedNamespaces)
    {
        struct RegularAttribute
        {
            std::string qname;
            std::string value;
            std::string namespaceUri;
            std::string localName;
        };

        NamespaceMap effectiveNamespaces = inScopeNamespaces;
        std::vector<RegularAttribute> regularAttributes;

        for (auto attr = node.first_attribute(); attr; attr = attr.next_attribute())
        {
            const std::string name = attr.name();

            if (isNamespaceDeclaration(name))
            {
                effectiveNamespaces[namespaceDeclarationPrefix(name)] = attr.value();
                continue;
            }

            const std::string prefix = qnamePrefix(name);

            regularAttributes.push_back({
                name,
                attr.value(),
                namespaceUriForPrefix(prefix, effectiveNamespaces),
                qnameLocalName(name)
            });
        }

        std::sort(
            regularAttributes.begin(),
            regularAttributes.end(),
            [](const RegularAttribute& a, const RegularAttribute& b) {
                if (a.namespaceUri != b.namespaceUri)
                {
                    return a.namespaceUri < b.namespaceUri;
                }

                return a.localName < b.localName;
            }
        );

        NamespaceMap renderedHere = renderedNamespaces;

        out += '<';
        out += node.name();

        for (const auto& [prefix, uri] : effectiveNamespaces)
        {
            if (!shouldRenderNamespace(prefix, uri, renderedHere)) continue;

            appendNamespaceDeclaration(out, prefix, uri);
            renderedHere[prefix] = uri;
        }

        for (const auto& attr : regularAttributes)
        {
            out += ' ';
            out += attr.qname;
            out += "=\"";
            escapeAttr(attr.value, out);
            out += '"';
        }

        out += '>';

        for (auto child = node.first_child(); child; child = child.next_sibling())
        {
            serializeNode(child, out, effectiveNamespaces, renderedHere);
        }

        out += "</";
        out += node.name();
        out += '>';
    }

    C14nSerializer::NamespaceMap
    C14nSerializer::collectAncestorNamespaces(const pugi::xml_node& node)
    {
        NamespaceMap namespaces;
        std::vector<pugi::xml_node> ancestors;

        for (auto parent = node.parent();
             parent && parent.type() != pugi::node_document;
             parent = parent.parent())
        {
            if (parent.type() == pugi::node_element)
            {
                ancestors.push_back(parent);
            }
        }

        std::reverse(ancestors.begin(), ancestors.end());

        for (const auto& ancestor : ancestors)
        {
            for (auto attr = ancestor.first_attribute(); attr; attr = attr.next_attribute())
            {
                const std::string name = attr.name();

                if (isNamespaceDeclaration(name))
                {
                    namespaces[namespaceDeclarationPrefix(name)] = attr.value();
                }
            }
        }

        return namespaces;
    }

    void
    C14nSerializer::escapeText(std::string_view text, std::string& out)
    {
        for (const char c : text)
        {
            switch (c)
            {
                case '&':  out += "&amp;"; break;
                case '<':  out += "&lt;";  break;
                case '>':  out += "&gt;";  break;
                case '\r': out += "&#xD;"; break;
                default:   out += c;       break;
            }
        }
    }

    void
    C14nSerializer::escapeAttr(std::string_view text, std::string& out)
    {
        for (const char c : text)
        {
            switch (c)
            {
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

}
