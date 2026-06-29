// xml_dsig_signer.cc
//
// Created by Saadat Baig on 29.06.26.
//
#include "xml_dsig_signer.h"
#include "xml_namespaces.h"
#include "c14n_serializer.h"
#include "utils/crypto_utils.h"

#include <openssl/evp.h>
#include <openssl/err.h>

#include <sstream>
#include <span>
#include <vector>

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
    }

    XmlDsigSigner::Result<std::string> XmlDsigSigner::sign(
        const pugi::xml_document& serviceMetadataDoc,
        EVP_PKEY* privateKey,
        const std::string& signingCertPem)
    {
        if (!privateKey) {
            return tl::unexpected(Error{"privateKey must not be null"});
        }

        pugi::xml_document finalDoc;

        auto xmlDecl = finalDoc.prepend_child(pugi::node_declaration);
        xmlDecl.append_attribute("version") = "1.0";
        xmlDecl.append_attribute("encoding") = "UTF-8";
        xmlDecl.append_attribute("standalone") = "no";

        auto ssm = finalDoc.append_child("smp:SignedServiceMetadata");
        ssm.append_attribute("xmlns:ds") = XmlNs::DSIG;
        ssm.append_attribute("xmlns:id") = XmlNs::ID;
        ssm.append_attribute("xmlns:smp") = XmlNs::SMP;
        ssm.append_attribute("xmlns:wsa") = XmlNs::WSA;

        for (auto child = serviceMetadataDoc.first_child(); child; child = child.next_sibling()) {
            if (child.type() == pugi::node_declaration) continue;
            ssm.append_copy(child);
        }

        const std::string canonicalReference = C14nSerializer::serialize(finalDoc);

        auto digestResult = sha256Base64(canonicalReference);
        if (!digestResult) {
            return tl::unexpected(digestResult.error());
        }

        auto signature = ssm.append_child("ds:Signature");
        //signature.append_attribute("xmlns") = XmlNs::DSIG;

        auto signedInfo = signature.append_child("ds:SignedInfo");

        auto c14nMethod = signedInfo.append_child("ds:CanonicalizationMethod");
        c14nMethod.append_attribute("Algorithm") = XmlNs::W3C_C14N;

        auto signatureMethod = signedInfo.append_child("ds:SignatureMethod");
        signatureMethod.append_attribute("Algorithm") = XmlNs::W3C_RSA_SHA256;

        auto reference = signedInfo.append_child("ds:Reference");
        reference.append_attribute("URI") = "";

        auto transforms = reference.append_child("ds:Transforms");

        auto transform = transforms.append_child("ds:Transform");
        transform.append_attribute("Algorithm") = XmlNs::W3C_ENVELOPED;

        auto digestMethod = reference.append_child("ds:DigestMethod");
        digestMethod.append_attribute("Algorithm") = XmlNs::W3C_SHA256;

        reference.append_child("ds:DigestValue")
            .text()
            .set(digestResult->c_str());

        const std::string canonicalSignedInfo = C14nSerializer::serialize(signedInfo);

        auto signatureResult = rsaSha256Base64(canonicalSignedInfo, privateKey);
        if (!signatureResult) {
            return tl::unexpected(signatureResult.error());
        }

        signature.append_child("ds:SignatureValue")
            .text()
            .set(signatureResult->c_str());

        auto keyInfo = signature.append_child("ds:KeyInfo");
        auto x509Data = keyInfo.append_child("ds:X509Data");

        auto subjectResult = CryptoUtils::certSubjectUID(signingCertPem);
        if (subjectResult) {
            x509Data.append_child("ds:X509SubjectName")
              .text()
              .set(subjectResult->c_str());
        }

        x509Data.append_child("ds:X509Certificate")
            .text()
            .set(stripPemHeaders(signingCertPem).c_str());

        std::ostringstream oss;
        finalDoc.save(oss, "", pugi::format_raw, pugi::encoding_utf8);

        return oss.str();
    }

    std::string XmlDsigSigner::stripPemHeaders(const std::string& pem) {
        std::string result;
        result.reserve(pem.size());

        std::istringstream iss(pem);
        std::string line;

        while (std::getline(iss, line)) {
            if (line.find("-----") != std::string::npos) continue;

            while (!line.empty() &&
                   (line.back() == '\r' ||
                    line.back() == '\n' ||
                    line.back() == ' ' ||
                    line.back() == '\t'))
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
        if (!ctx) {
            return tl::unexpected(Error{"Failed to create EVP_MD_CTX"});
        }

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
        EVP_PKEY* key)
    {
        EVP_MD_CTX* ctx = EVP_MD_CTX_new();
        if (!ctx) {
            return tl::unexpected(Error{"Failed to create EVP_MD_CTX for signing"});
        }

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

        size_t signatureLen = 0;

        if (EVP_DigestSignFinal(ctx, nullptr, &signatureLen) != 1) {
            EVP_MD_CTX_free(ctx);
            return tl::unexpected(Error{
                "EVP_DigestSignFinal length failed: " + opensslErrors()
            });
        }

        std::vector<uint8_t> signature(signatureLen);

        if (EVP_DigestSignFinal(ctx, signature.data(), &signatureLen) != 1) {
            EVP_MD_CTX_free(ctx);
            return tl::unexpected(Error{
                "EVP_DigestSignFinal sign failed: " + opensslErrors()
            });
        }

        EVP_MD_CTX_free(ctx);

        signature.resize(signatureLen);

        return CryptoUtils::base64Encode(
            std::span<const uint8_t>(signature.data(), signature.size())
        );
    }
}
