//
// Created by Saadat Baig on 02.07.26.
//
#pragma once

#include <pugixml.hpp>

#include <stdexcept>
#include <string>
#include <string_view>

#include "xml_namespaces.h"


namespace SMPPlusPlus::Pugi
{
    constexpr const char* DEFAULT_PREFIX = "smppp";
    constexpr const char* DEFAULT_NS_PREFIX = "xmlns:smppp";


    inline bool isValidXmlNameStart(char c) {
        return (c >= 'A' && c <= 'Z') ||
               (c >= 'a' && c <= 'z') ||
               c == '_';
    }

    inline bool isValidXmlNameChar(char c) {
        return isValidXmlNameStart(c) ||
               (c >= '0' && c <= '9') ||
               c == '-' ||
               c == '.';
    }

    inline void validateXmlPrefix(std::string_view prefix) {
        if (prefix.empty()) {
            throw std::invalid_argument("XML namespace prefix must not be empty");
        }

        if (prefix == "xml" || prefix == "xmlns") {
            throw std::invalid_argument("Reserved XML namespace prefix");
        }

        if (!isValidXmlNameStart(prefix.front())) {
            throw std::invalid_argument("Invalid XML namespace prefix start character");
        }

        for (char c : prefix) {
            if (!isValidXmlNameChar(c)) {
                throw std::invalid_argument("Invalid XML namespace prefix character");
            }
        }
    }

    inline void validateXmlLocalName(std::string_view localName) {
        if (localName.empty()) {
            throw std::invalid_argument("XML local name must not be empty");
        }

        if (!isValidXmlNameStart(localName.front())) {
            throw std::invalid_argument("Invalid XML local name start character");
        }

        for (char c : localName) {
            if (!isValidXmlNameChar(c)) {
                throw std::invalid_argument("Invalid XML local name character");
            }
        }
    }

    inline std::string xmlnsAttributeName(std::string_view prefix) {
        std::string name;
        name.reserve(6 + prefix.size());
        name += "xmlns:";
        name += prefix;
        return name;
    }

    inline std::string qualifiedName(
        std::string_view prefix,
        std::string_view localName)
    {
        validateXmlPrefix(prefix);
        validateXmlLocalName(localName);

        std::string qname;
        qname.reserve(prefix.size() + 1 + localName.size());
        qname += prefix;
        qname += ':';
        qname += localName;
        return qname;
    }

    inline bool hasNamespaceInScope(
        pugi::xml_node node,
        std::string_view prefix,
        std::string_view uri)
    {
        const std::string attrName = xmlnsAttributeName(prefix);

        for (pugi::xml_node current = node;
             current && current.type() == pugi::node_element;
             current = current.parent())
        {
            pugi::xml_attribute attr = current.attribute(attrName.c_str());

            if (!attr) continue;

            return uri == attr.value();
        }

        return false;
    }

    inline void ensureNamespaceDeclaration(
        pugi::xml_node node,
        std::string_view prefix,
        std::string_view uri)
    {
        if (!node || node.type() != pugi::node_element) {
            throw std::invalid_argument("Namespace declaration target must be an XML element");
        }

        validateXmlPrefix(prefix);

        if (uri.empty()) {
            throw std::invalid_argument("Namespace URI must not be empty");
        }

        if (hasNamespaceInScope(node, prefix, uri)) {
            return;
        }

        const std::string attrName = xmlnsAttributeName(prefix);

        pugi::xml_attribute existing = node.attribute(attrName.c_str());
        if (existing) {
            if (uri != existing.value()) {
                throw std::invalid_argument("Namespace prefix already declared with different URI");
            }

            return;
        }

        node.append_attribute(attrName.c_str()) = std::string(uri).c_str();
    }

    inline pugi::xml_node appendNamespacedChild(
        pugi::xml_node parent,
        std::string_view prefix,
        std::string_view uri,
        std::string_view localName)
    {
        if (!parent || parent.type() != pugi::node_element) {
            throw std::invalid_argument("Parent must be an XML element");
        }

        ensureNamespaceDeclaration(parent, prefix, uri);

        const std::string qname = qualifiedName(prefix, localName);
        return parent.append_child(qname.c_str());
    }

    inline pugi::xml_node
    appendNamespacedChildFast(pugi::xml_node parent, std::string_view name, bool includeNamespaceUrl)
    {
        std::string qname = DEFAULT_PREFIX;
        qname += ":";
        qname += name;

        pugi::xml_node generated = parent.append_child(qname.c_str());

        if (includeNamespaceUrl)
        {
            generated.append_attribute(DEFAULT_NS_PREFIX) = SMPPlusPlus::XmlNs::SMP_PLUS_PLUS;
        }

        return generated;
    }

}
