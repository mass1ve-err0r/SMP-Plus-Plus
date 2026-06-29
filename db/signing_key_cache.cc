//
// Created by Saadat Baig on 29.06.26.
//
#include "signing_key_cache.h"
#include "certificate_repository.h"
#include "utils/crypto_utils.h"
#include "utils/environment.h"

#include <libpq-fe.h>
#include <openssl/pem.h>
#include <openssl/bio.h>
#include <iostream>
#include <stdexcept>


namespace SMPPlusPlus
{
    SigningKeyCache& SigningKeyCache::instance() {
        static SigningKeyCache inst;
        return inst;
    }

    // -----------------------------------------------------------------------------
    // loadFromDb - synchronous via libpq, for startup use before app.run()
    // -----------------------------------------------------------------------------
    void SigningKeyCache::loadFromDb(const std::string& connString) {
        PGconn* conn = PQconnectdb(connString.c_str());
        if (PQstatus(conn) != CONNECTION_OK) {
            std::string err = PQerrorMessage(conn);
            PQfinish(conn);
            std::cerr << "[SigningKeyCache] DB connection failed: " << err << "\n";
            return;
        }

        PGresult* res = PQexec(conn,
            "SELECT id, cert_pem, private_key_enc FROM smp_certificates "
            "WHERE is_active = true LIMIT 1"
        );

        if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0) {
            PQclear(res);
            PQfinish(conn);
            std::cout << "[SigningKeyCache] No active SMP certificate found. "
                         "Upload and activate a certificate to enable signing.\n";
            return;
        }

        const std::string certId      = PQgetvalue(res, 0, 0);
        const std::string certPem     = PQgetvalue(res, 0, 1);
        const std::string encryptedKey = PQgetvalue(res, 0, 2);
        PQclear(res);
        PQfinish(conn);

        loadEntry(certId, certPem, encryptedKey);
    }

    // -----------------------------------------------------------------------------
    // reloadAsync - coroutine, for use after app.run() (cert activation)
    // -----------------------------------------------------------------------------
    drogon::Task<void> SigningKeyCache::reloadAsync() {
        auto cert = co_await CertificateRepository::findActive();
        if (!cert) {
            invalidate();
            std::cout << "[SigningKeyCache] No active certificate - cache cleared.\n";
            co_return;
        }
        loadEntry(cert->id, cert->certPem, cert->privateKeyEnc);
    }

    // -----------------------------------------------------------------------------
    // loadEntry - decrypt key and cache EVP_PKEY*
    // -----------------------------------------------------------------------------
    void SigningKeyCache::loadEntry(
        const std::string& certId,
        const std::string& certPem,
        const std::string& encryptedKey)
    {
        // Decrypt private key with master key
        const auto masterKey = Environment::masterKeyBytes();
        auto decryptResult = CryptoUtils::aesDecrypt(encryptedKey, masterKey);
        if (!decryptResult) {
            std::cerr << "[SigningKeyCache] Failed to decrypt private key: "
                      << decryptResult.error().message << "\n";
            return;
        }

        // Parse PEM private key into EVP_PKEY*
        const std::string& pemKey = *decryptResult;
        BIO* bio = BIO_new_mem_buf(pemKey.data(), static_cast<int>(pemKey.size()));
        if (!bio) {
            std::cerr << "[SigningKeyCache] Failed to create BIO for private key\n";
            return;
        }

        EVP_PKEY* rawKey = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr);
        BIO_free(bio);

        if (!rawKey) {
            std::cerr << "[SigningKeyCache] Failed to parse private key PEM\n";
            return;
        }

        // Wrap in shared_ptr with custom deleter
        auto keyPtr = std::shared_ptr<EVP_PKEY>(rawKey, EVP_PKEY_free);

        Entry entry;
        entry.key     = std::move(keyPtr);
        entry.certPem = certPem;
        entry.certId  = certId;

        {
            std::unique_lock lock(mutex_);
            entry_ = std::move(entry);
        }

        std::cout << "[SigningKeyCache] Active SMP signing key loaded (cert: "
                  << certId << ")\n";
    }

    std::optional<SigningKeyCache::Entry> SigningKeyCache::get() const {
        std::shared_lock lock(mutex_);
        return entry_;
    }

    bool SigningKeyCache::ready() const {
        std::shared_lock lock(mutex_);
        return entry_.has_value();
    }

    void SigningKeyCache::invalidate() {
        std::unique_lock lock(mutex_);
        entry_.reset();
    }

} // namespace SMPPlusPlus
