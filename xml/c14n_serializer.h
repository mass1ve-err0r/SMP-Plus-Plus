//
// Created by Saadat Baig on 29.06.26.
//
#pragma once

#include <string>
#include <pugixml.hpp>


namespace SMPPlusPlus
{
    /**
     * C14nSerializer - Inclusive Canonical XML serialization.
     *
     * Implements a subset of W3C Inclusive C14N
     * (http://www.w3.org/TR/2001/REC-xml-c14n-20010315) sufficient for
     * Peppol SMP XML-DSIG signing.
     *
     * Since we generate all XML ourselves (we never parse arbitrary input),
     * the document is already structurally correct. C14N here means:
     *   - No XML declaration in the canonicalized output
     *   - Namespace declarations rendered on each element that uses them
     *   - Attribute order: namespace declarations (alphabetical by prefix),
     *     then regular attributes (alphabetical by name)
     *   - No self-closing tags (empty elements as <Foo></Foo>)
     *   - Whitespace in text content preserved as-is
     *   - Special characters in text/attribute values escaped:
     *     & -> &amp;  < -> &lt;  > -> &gt;  " -> &quot;
     *
     * Thread safety: all methods are stateless and safe to call concurrently.
     */
    class C14nSerializer {
    public:
        C14nSerializer() = delete;

        /**
         * Serialize a pugi::xml_node (or document) to canonical XML string.
         * The output is suitable for SHA-256 digesting in XML-DSIG.
         */
        static std::string serialize(const pugi::xml_document& doc);
        static std::string serialize(const pugi::xml_node& node);

    private:
        static void serializeNode(
            const pugi::xml_node& node,
            std::string& out
        );

        static void serializeElement(
            const pugi::xml_node& node,
            std::string& out
        );

        static void escapeText(std::string_view text, std::string& out);
        static void escapeAttr(std::string_view text, std::string& out);
    };

} // namespace SMPPlusPlus
