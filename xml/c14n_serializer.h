//
// Created by Saadat Baig on 29.06.26.
//
#pragma once

#include <pugixml.hpp>

#include <map>
#include <string>
#include <string_view>


namespace SMPPlusPlus
{
    class C14nSerializer
    {
    public:

        C14nSerializer() = delete;

        static std::string
        serialize(const pugi::xml_document& doc);

        static std::string
        serialize(const pugi::xml_node& node);

        using NamespaceMap = std::map<std::string, std::string>;

        static void
        escapeText(std::string_view text, std::string& out);

        static void
        escapeAttr(std::string_view text, std::string& out);

    private:

        static void
        serializeNode(const pugi::xml_node& node, std::string& out, const NamespaceMap& inScopeNamespaces, const NamespaceMap& renderedNamespaces);

        static void
        serializeElement(const pugi::xml_node& node, std::string& out, const NamespaceMap& inScopeNamespaces,const NamespaceMap& renderedNamespaces);

        static NamespaceMap
        collectAncestorNamespaces(const pugi::xml_node& node);

    };

}
