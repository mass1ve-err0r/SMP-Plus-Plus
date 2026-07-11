//
// Created by Saadat Baig on 29.06.26.
//
#pragma once

#include <pugixml.hpp>
#include <openssl/evp.h>
#include <tl/expected.hpp>

#include <string>


namespace SMPPlusPlus
{
    /**
     * XmlDsigSigner - XML-DSIG enveloped signature for Peppol SMP responses.
     *
     * Takes a ServiceMetadata document and produces a SignedServiceMetadata
     * document with an RSA-SHA256 enveloped signature per:
     *   http://www.w3.org/TR/xmldsig-core/
     *
     * Algorithm:
     *   CanonicalizationMethod: http://www.w3.org/TR/2001/REC-xml-c14n-20010315
     *   SignatureMethod:        http://www.w3.org/2001/04/xmldsig-more#rsa-sha256
     *   DigestMethod:           http://www.w3.org/2001/04/xmlenc#sha256
     *   Transform:              enveloped-signature
     *
     * Thread safety: all methods are stateless. EVP_PKEY* must remain valid
     * for the duration of the call. The caller (SigningKeyCache) ensures this
     * via shared_ptr ownership during the sign operation.
     */
    class XmlDsigSigner
    {
    public:

        XmlDsigSigner() = delete;

        struct Error
        {
            std::string message;
        };

        template<typename T>
        using Result = tl::expected<T, Error>;

        /**
         * Sign a ServiceMetadata document and return a complete
         * SignedServiceMetadata XML string.
         *
         * @param serviceMetadataDoc  Document from ServiceMetadataBuilder::build()
         * @param privateKey          RSA private key for signing (from SigningKeyCache)
         * @param signingCertPem      Public cert PEM to embed in KeyInfo
         * @return                    Complete SignedServiceMetadata XML string
         */
        static Result<std::string>
        sign(const pugi::xml_document& serviceMetadataDoc, EVP_PKEY* privateKey, const std::string& signingCertPem);

    private:

        // Strip PEM headers to get raw base64 for embedding in XML
        static std::string
        stripPemHeaders(const std::string& pem);

        // Compute SHA-256 digest and return as base64
        static Result<std::string>
        sha256Base64(const std::string& data);

        // RSA-SHA256 sign and return as base64
        static Result<std::string>
        rsaSha256Base64(const std::string& data, EVP_PKEY* key);
    };

} // namespace SMPPlusPlus
