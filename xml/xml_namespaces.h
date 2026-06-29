//
// Created by Saadat Baig on 29.06.26.
//
#pragma once


namespace SMPPlusPlus::XmlNs
{
    // Peppol SMP namespaces
    constexpr const char* SMP  = "http://busdox.org/serviceMetadata/publishing/1.0/";
    constexpr const char* ID   = "http://busdox.org/transport/identifiers/1.0/";
    constexpr const char* WSA  = "http://www.w3.org/2005/08/addressing";

    constexpr const char* DSIG = "http://www.w3.org/2000/09/xmldsig#";
    constexpr const char* W3C_C14N = "http://www.w3.org/TR/2001/REC-xml-c14n-20010315";
    constexpr const char* W3C_ENVELOPED = "http://www.w3.org/2000/09/xmldsig#enveloped-signature";
    constexpr const char* W3C_RSA_SHA256 = "http://www.w3.org/2001/04/xmldsig-more#rsa-sha256";
    constexpr const char* W3C_SHA256 = "http://www.w3.org/2001/04/xmlenc#sha256";


} // namespace SMPPlusPlus::XmlNs
