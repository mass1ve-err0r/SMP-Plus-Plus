//
// Created by Saadat Baig on 29.06.26.
//
#include "sml_registration_service.h"

#include <curl/curl.h>
#include <sstream>
#include <iostream>
#include <fstream>


namespace SMPPlusPlus
{
    namespace {

        // libcurl write callback - accumulates response body
        size_t writeCallback(char* ptr, size_t size, size_t nmemb, std::string* data) {
            data->append(ptr, size * nmemb);
            return size * nmemb;
        }

        // Write cert/key to temp files for libcurl mutual TLS
        // libcurl requires file paths for client cert/key, not in-memory PEM
        struct TlsFiles {
            std::string certPath;
            std::string keyPath;

            ~TlsFiles() {
                if (!certPath.empty()) std::remove(certPath.c_str());
                if (!keyPath.empty())  std::remove(keyPath.c_str());
            }
        };

        tl::expected<TlsFiles, SmlRegistrationService::Error> writeTlsFiles(
            const std::string& certPem,
            const std::string& privateKeyPem)
        {
            TlsFiles files;

            // Write cert
            files.certPath = "/tmp/smp_cert_" + std::to_string(
                std::chrono::steady_clock::now().time_since_epoch().count()) + ".pem";
            files.keyPath = files.certPath + ".key";

            {
                std::ofstream f(files.certPath);
                if (!f) return tl::unexpected(
                    SmlRegistrationService::Error{"Failed to write temp cert file"});
                f << certPem;
            }
            {
                std::ofstream f(files.keyPath);
                if (!f) return tl::unexpected(
                    SmlRegistrationService::Error{"Failed to write temp key file"});
                f << privateKeyPem;
            }

            return files;
        }

    } // anonymous namespace

    // -----------------------------------------------------------------------------
    // SOAP envelope builders
    // The SML management API uses a minimal SOAP 1.1 envelope.
    // The exact WSDL is from OpenPeppol's SML specification.
    // -----------------------------------------------------------------------------
    std::string SmlRegistrationService::buildRegisterEnvelope(
        const std::string& smpId)
    {
        std::ostringstream oss;
        oss << R"(<?xml version="1.0" encoding="UTF-8"?>)"
            << R"(<soapenv:Envelope)"
            << R"( xmlns:soapenv="http://schemas.xmlsoap.org/soap/envelope/")"
            << R"( xmlns:man="http://busdox.org/serviceMetadata/managementservice/1.0/">)"
            << R"(<soapenv:Header/>)"
            << R"(<soapenv:Body>)"
            << R"(<man:Create>)"
            << R"(<man:ServiceMetadataPublisherID>)" << smpId
            << R"(</man:ServiceMetadataPublisherID>)"
            << R"(</man:Create>)"
            << R"(</soapenv:Body>)"
            << R"(</soapenv:Envelope>)";
        return oss.str();
    }

    std::string SmlRegistrationService::buildDeregisterEnvelope(
        const std::string& smpId)
    {
        std::ostringstream oss;
        oss << R"(<?xml version="1.0" encoding="UTF-8"?>)"
            << R"(<soapenv:Envelope)"
            << R"( xmlns:soapenv="http://schemas.xmlsoap.org/soap/envelope/")"
            << R"( xmlns:man="http://busdox.org/serviceMetadata/managementservice/1.0/">)"
            << R"(<soapenv:Header/>)"
            << R"(<soapenv:Body>)"
            << R"(<man:Delete>)"
            << R"(<man:ServiceMetadataPublisherID>)" << smpId
            << R"(</man:ServiceMetadataPublisherID>)"
            << R"(</man:Delete>)"
            << R"(</soapenv:Body>)"
            << R"(</soapenv:Envelope>)";
        return oss.str();
    }

    // -----------------------------------------------------------------------------
    // callSml - POST SOAP envelope with mutual TLS via libcurl
    // -----------------------------------------------------------------------------
    SmlRegistrationService::Result<void> SmlRegistrationService::callSml(
        const std::string& smlEndpoint,
        const std::string& soapAction,
        const std::string& envelope,
        const std::string& certPem,
        const std::string& privateKeyPem)
    {
        // Write cert/key to temp files for libcurl
        auto tlsFiles = writeTlsFiles(certPem, privateKeyPem);
        if (!tlsFiles) return tl::unexpected(tlsFiles.error());

        CURL* curl = curl_easy_init();
        if (!curl) return tl::unexpected(Error{"Failed to initialize libcurl"});

        std::string responseBody;
        long httpCode = 0;

        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: text/xml; charset=utf-8");
        const std::string soapActionHeader = "SOAPAction: \"" + soapAction + "\"";
        headers = curl_slist_append(headers, soapActionHeader.c_str());

        curl_easy_setopt(curl, CURLOPT_URL,           smlEndpoint.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER,    headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS,    envelope.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(envelope.size()));
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA,     &responseBody);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT,       30L);

        // Mutual TLS
        curl_easy_setopt(curl, CURLOPT_SSLCERT,     tlsFiles->certPath.c_str());
        curl_easy_setopt(curl, CURLOPT_SSLKEY,      tlsFiles->keyPath.c_str());
        curl_easy_setopt(curl, CURLOPT_SSLCERTTYPE, "PEM");
        curl_easy_setopt(curl, CURLOPT_SSLKEYTYPE,  "PEM");

        // Verify SML server cert
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

        const CURLcode res = curl_easy_perform(curl);
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK) {
            return tl::unexpected(Error{
                std::string("SML SOAP call failed: ") + curl_easy_strerror(res),
                0
            });
        }

        if (httpCode < 200 || httpCode >= 300) {
            return tl::unexpected(Error{
                "SML returned HTTP " + std::to_string(httpCode) +
                ": " + responseBody,
                static_cast<int>(httpCode)
            });
        }

        return {};
    }

    // -----------------------------------------------------------------------------
    // Public API
    // -----------------------------------------------------------------------------
    SmlRegistrationService::Result<void> SmlRegistrationService::registerSmp(
        const std::string& smlEndpoint,
        const std::string& smpId,
        const std::string& certPem,
        const std::string& privateKeyPem)
    {
        if (smpId.empty())
            return tl::unexpected(Error{
                "smp.id is not configured. Set it in Settings before registering."
            });

        return callSml(
            smlEndpoint,
            "http://busdox.org/serviceMetadata/managementservice/1.0/ManageServiceMetadataService/create",
            buildRegisterEnvelope(smpId),
            certPem,
            privateKeyPem
        );
    }

    SmlRegistrationService::Result<void> SmlRegistrationService::deregisterSmp(
        const std::string& smlEndpoint,
        const std::string& smpId,
        const std::string& certPem,
        const std::string& privateKeyPem)
    {
        if (smpId.empty())
            return tl::unexpected(Error{
                "smp.id is not configured."
            });

        return callSml(
            smlEndpoint,
            "http://busdox.org/serviceMetadata/managementservice/1.0/ManageServiceMetadataService/delete",
            buildDeregisterEnvelope(smpId),
            certPem,
            privateKeyPem
        );
    }

} // namespace SMPPlusPlus
