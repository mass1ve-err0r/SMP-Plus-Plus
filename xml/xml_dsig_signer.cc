//
// Created by Saadat Baig on 29.06.26.
//
#include "xml_dsig_signer.h"
#include "xml_namespaces.h"
#include "c14n_serializer.h"
#include "utils/crypto_utils.h"

#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>

#include <sstream>


namespace SMPPlusPlus
{
    namespace {

        std::string opensslErrors() {
            std::string result;
            unsigned long err;
            char buf[256];
            while ((err = ERR_get_error()) != 0) {
                ERR_error_string_n(err, buf, sizeof(buf));
                if (!result.empty()) result += "; ";
                result += buf;
            }
            return result.empty() ? "unknown OpenSSL error" : result;
        }

    } // anonymous namespace

    XmlDsigSigner::Result<std::string> XmlDsigSigner::sign(
        const pugi::xml_document& serviceMetadataDoc,
        EVP_PKEY*                 privateKey,
        const std::string&        signingCertPem)
    {
        // -------------------------------------------------------------------------
        // Step 1: Canonicalize the ServiceMetadata element
        // -------------------------------------------------------------------------
        const std::string canonicalized = C14nSerializer::serialize(serviceMetadataDoc);

        // -------------------------------------------------------------------------
        // Step 2: SHA-256 digest the canonicalized ServiceMetadata
        // -------------------------------------------------------------------------
        auto digestResult = sha256Base64(canonicalized);
        if (!digestResult) return tl::unexpected(digestResult.error());

        // -------------------------------------------------------------------------
        // Step 3: Build SignedInfo element and canonicalize it
        // The signature covers the canonicalized SignedInfo element.
        // -------------------------------------------------------------------------
        pugi::xml_document signedInfoDoc;
        auto signedInfo = signedInfoDoc.append_child("SignedInfo");

        auto c14nMethod = signedInfo.append_child("CanonicalizationMethod");
        c14nMethod.append_attribute("Algorithm") = XmlNs::W3C_C14N;

        auto sigMethod = signedInfo.append_child("SignatureMethod");
        sigMethod.append_attribute("Algorithm") = XmlNs::W3C_RSA_SHA256;

        auto reference = signedInfo.append_child("Reference");
        reference.append_attribute("URI") = "";

        auto transforms = reference.append_child("Transforms");
        auto transform  = transforms.append_child("Transform");
        transform.append_attribute("Algorithm") = XmlNs::W3C_ENVELOPED;

        auto digestMethod = reference.append_child("DigestMethod");
        digestMethod.append_attribute("Algorithm") = XmlNs::W3C_SHA256;

        reference.append_child("DigestValue")
                 .text().set(digestResult->c_str());

        const std::string canonicalSignedInfo =
            C14nSerializer::serialize(signedInfoDoc.first_child());

        // -------------------------------------------------------------------------
        // Step 4: RSA-SHA256 sign the canonicalized SignedInfo
        // -------------------------------------------------------------------------
        auto sigResult = rsaSha256Base64(canonicalSignedInfo, privateKey);
        if (!sigResult) return tl::unexpected(sigResult.error());

        // -------------------------------------------------------------------------
        // Step 5: Build complete SignedServiceMetadata document
        // -------------------------------------------------------------------------
        pugi::xml_document finalDoc;

        auto xmlDecl = finalDoc.prepend_child(pugi::node_declaration);
        xmlDecl.append_attribute("version")  = "1.0";
        xmlDecl.append_attribute("encoding") = "UTF-8";

        // <SignedServiceMetadata>
        auto ssm = finalDoc.append_child("smp:SignedServiceMetadata");
        ssm.append_attribute("xmlns:ds") = XmlNs::DSIG;
        ssm.append_attribute("xmlns:id") = XmlNs::ID;
        ssm.append_attribute("xmlns:smp") = XmlNs::SMP;
        ssm.append_attribute("xmlns:wsa") = XmlNs::WSA;

        // Copy ServiceMetadata content from the input document
        for (auto child = serviceMetadataDoc.first_child(); child;
             child = child.next_sibling())
        {
            if (child.type() == pugi::node_declaration) continue;
            ssm.append_copy(child);
        }

        // <ds:Signature>
        auto sig = ssm.append_child("Signature");
        sig.append_attribute("xmlns") = XmlNs::DSIG;

        // Copy SignedInfo into Signature
        sig.append_copy(signedInfoDoc.first_child());

        // <ds:SignatureValue>
        sig.append_child("SignatureValue")
           .text().set(sigResult->c_str());

        // <ds:KeyInfo>
        auto keyInfo = sig.append_child("KeyInfo");
        auto x509Data = keyInfo.append_child("X509Data");
        x509Data.append_child("X509Certificate")
                .text().set(stripPemHeaders(signingCertPem).c_str());

        // -------------------------------------------------------------------------
        // Step 6: Serialize final document
        // -------------------------------------------------------------------------
        std::ostringstream oss;
        finalDoc.save(oss, "  ", pugi::format_raw, pugi::encoding_utf8);
        return oss.str();
    }

    std::string XmlDsigSigner::stripPemHeaders(const std::string& pem) {
        std::string result;
        std::istringstream iss(pem);
        std::string line;
        while (std::getline(iss, line)) {
            if (line.find("-----") != std::string::npos) continue;
            while (!line.empty() &&
                   (line.back() == '\r' || line.back() == '\n' || line.back() == ' '))
            {
                line.pop_back();
            }
            result += line;
        }
        return result;
    }

    XmlDsigSigner::Result<std::string> XmlDsigSigner::sha256Base64(
        const std::string& data)
    {
        EVP_MD_CTX* ctx = EVP_MD_CTX_new();
        if (!ctx)
            return tl::unexpected(Error{"Failed to create EVP_MD_CTX"});

        std::vector<uint8_t> digest(EVP_MAX_MD_SIZE);
        unsigned int digestLen = 0;

        if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1 ||
            EVP_DigestUpdate(ctx, data.data(), data.size()) != 1 ||
            EVP_DigestFinal_ex(ctx, digest.data(), &digestLen) != 1)
        {
            EVP_MD_CTX_free(ctx);
            return tl::unexpected(Error{"SHA-256 digest failed: " + opensslErrors()});
        }
        EVP_MD_CTX_free(ctx);

        digest.resize(digestLen);
        return CryptoUtils::base64Encode(
            std::span<const uint8_t>(digest.data(), digest.size())
        );
    }

    XmlDsigSigner::Result<std::string> XmlDsigSigner::rsaSha256Base64(
        const std::string& data,
        EVP_PKEY*          key)
    {
        EVP_MD_CTX* ctx = EVP_MD_CTX_new();
        if (!ctx)
            return tl::unexpected(Error{"Failed to create EVP_MD_CTX for signing"});

        if (EVP_DigestSignInit(ctx, nullptr, EVP_sha256(), nullptr, key) != 1) {
            EVP_MD_CTX_free(ctx);
            return tl::unexpected(Error{
                "EVP_DigestSignInit failed: " + opensslErrors()
            });
        }

        if (EVP_DigestSignUpdate(ctx, data.data(), data.size()) != 1) {
            EVP_MD_CTX_free(ctx);
            return tl::unexpected(Error{
                "EVP_DigestSignUpdate failed: " + opensslErrors()
            });
        }

        // Get signature length
        size_t sigLen = 0;
        if (EVP_DigestSignFinal(ctx, nullptr, &sigLen) != 1) {
            EVP_MD_CTX_free(ctx);
            return tl::unexpected(Error{
                "EVP_DigestSignFinal (length) failed: " + opensslErrors()
            });
        }

        std::vector<uint8_t> signature(sigLen);
        if (EVP_DigestSignFinal(ctx, signature.data(), &sigLen) != 1) {
            EVP_MD_CTX_free(ctx);
            return tl::unexpected(Error{
                "EVP_DigestSignFinal (sign) failed: " + opensslErrors()
            });
        }
        EVP_MD_CTX_free(ctx);

        signature.resize(sigLen);
        return CryptoUtils::base64Encode(
            std::span<const uint8_t>(signature.data(), signature.size())
        );
    }

} // namespace SMPPlusPlus
