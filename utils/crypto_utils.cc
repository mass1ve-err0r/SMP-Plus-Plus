//
// Created by Saadat Baig on 28.06.26.
//
#include "crypto_utils.h"
#include <bcrypt/BCrypt.hpp>
#include <charconv>

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/sha.h>

#include <sstream>
#include <iomanip>
#include <cstring>
#include <array>

namespace SMPPlusPlus
{
    // -----------------------------------------------------------------------------
    // Internal helpers (file-local, not exposed)
    // -----------------------------------------------------------------------------
    namespace {

        // Drain the OpenSSL error queue and return a human-readable string.
        // Always call this on the same thread that made the failing OpenSSL call.
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

        // RAII wrapper for EVP_CIPHER_CTX
        struct CipherCtxGuard {
            EVP_CIPHER_CTX* ctx;
            explicit CipherCtxGuard() : ctx(EVP_CIPHER_CTX_new()) {}
            ~CipherCtxGuard() { if (ctx) EVP_CIPHER_CTX_free(ctx); }
            CipherCtxGuard(const CipherCtxGuard&) = delete;
            CipherCtxGuard& operator=(const CipherCtxGuard&) = delete;
            bool valid() const { return ctx != nullptr; }
        };

        // RAII wrapper for BIO chains
        struct BioGuard {
            BIO* bio;
            explicit BioGuard(BIO* b) : bio(b) {}
            ~BioGuard() { if (bio) BIO_free_all(bio); }
            BioGuard(const BioGuard&) = delete;
            BioGuard& operator=(const BioGuard&) = delete;
        };

        // RAII wrapper for X509
        struct X509Guard {
            X509* cert;
            explicit X509Guard(X509* c) : cert(c) {}
            ~X509Guard() { if (cert) X509_free(cert); }
            X509Guard(const X509Guard&) = delete;
            X509Guard& operator=(const X509Guard&) = delete;
        };

        // RAII wrapper for EVP_PKEY
        struct PkeyGuard {
            EVP_PKEY* key;
            explicit PkeyGuard(EVP_PKEY* k) : key(k) {}
            ~PkeyGuard() { if (key) EVP_PKEY_free(key); }
            PkeyGuard(const PkeyGuard&) = delete;
            PkeyGuard& operator=(const PkeyGuard&) = delete;
        };

        // Split a string by a delimiter character, returns up to maxParts parts.
        std::vector<std::string> splitString(std::string_view input, char delim, size_t maxParts = 0) {
            std::vector<std::string> parts;
            size_t start = 0;
            size_t pos;
            while ((pos = input.find(delim, start)) != std::string_view::npos) {
                if (maxParts > 0 && parts.size() + 1 >= maxParts) break;
                parts.emplace_back(input.substr(start, pos - start));
                start = pos + 1;
            }
            parts.emplace_back(input.substr(start));
            return parts;
        }

    } // anonymous namespace

    // -----------------------------------------------------------------------------
    // Base64
    // -----------------------------------------------------------------------------
    std::string CryptoUtils::base64Encode(std::span<const uint8_t> data) {
        BIO* b64 = BIO_new(BIO_f_base64());
        BIO* mem = BIO_new(BIO_s_mem());
        BIO_push(b64, mem);
        BioGuard guard(b64); // frees both via chain

        BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL); // no line breaks
        BIO_write(b64, data.data(), static_cast<int>(data.size()));
        BIO_flush(b64);

        BUF_MEM* bptr;
        BIO_get_mem_ptr(mem, &bptr);
        return std::string(bptr->data, bptr->length);
    }

    std::string CryptoUtils::base64Encode(std::string_view data) {
        return base64Encode(std::span<const uint8_t>(
            reinterpret_cast<const uint8_t*>(data.data()), data.size()
        ));
    }

    CryptoUtils::Result<std::vector<uint8_t>> CryptoUtils::base64Decode(std::string_view encoded) {
        BIO* b64 = BIO_new(BIO_f_base64());
        BIO* mem = BIO_new_mem_buf(encoded.data(), static_cast<int>(encoded.size()));
        BIO_push(b64, mem);
        BioGuard guard(b64);

        BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);

        std::vector<uint8_t> result(encoded.size()); // upper bound
        int bytesRead = BIO_read(b64, result.data(), static_cast<int>(result.size()));
        if (bytesRead < 0) {
            return tl::unexpected(Error{"Base64 decode failed: " + opensslErrors()});
        }
        result.resize(static_cast<size_t>(bytesRead));
        return result;
    }

    // -----------------------------------------------------------------------------
    // Hex
    // -----------------------------------------------------------------------------
    std::string CryptoUtils::hexEncode(std::span<const uint8_t> data) {
        std::ostringstream oss;
        oss << std::hex << std::setfill('0');
        for (uint8_t byte : data) {
            oss << std::setw(2) << static_cast<unsigned>(byte);
        }
        return oss.str();
    }

    CryptoUtils::Result<std::vector<uint8_t>> CryptoUtils::hexDecode(std::string_view hex) {
        if (hex.size() % 2 != 0) {
            return tl::unexpected(Error{"Hex string has odd length"});
        }
        std::vector<uint8_t> bytes;
        bytes.reserve(hex.size() / 2);
        for (size_t i = 0; i < hex.size(); i += 2) {
            uint8_t byte{};
            auto [ptr, ec] = std::from_chars(hex.data() + i, hex.data() + i + 2, byte, 16);
            if (ec != std::errc{}) {
                return tl::unexpected(Error{
                    std::string("Invalid hex at position ") + std::to_string(i)
                });
            }
            bytes.push_back(byte);
        }
        return bytes;
    }

    // -----------------------------------------------------------------------------
    // Random bytes
    // -----------------------------------------------------------------------------
    CryptoUtils::Result<std::vector<uint8_t>> CryptoUtils::randomBytes(size_t count) {
        std::vector<uint8_t> buf(count);
        if (RAND_bytes(buf.data(), static_cast<int>(count)) != 1) {
            return tl::unexpected(Error{"RAND_bytes failed: " + opensslErrors()});
        }
        return buf;
    }

    // -----------------------------------------------------------------------------
    // AES-256-GCM encrypt
    // -----------------------------------------------------------------------------
    CryptoUtils::Result<std::string> CryptoUtils::aesEncrypt(
        std::string_view plaintext,
        std::span<const uint8_t> keyBytes)
    {
        if (keyBytes.size() != 32) {
            return tl::unexpected(Error{"AES-256 requires exactly 32 key bytes"});
        }

        // Generate random 12-byte IV
        auto ivResult = randomBytes(IV_SIZE);
        if (!ivResult) return tl::unexpected(ivResult.error());
        const auto& iv = *ivResult;

        // Prepare ciphertext buffer (same size as plaintext for GCM)
        std::vector<uint8_t> ciphertext(plaintext.size());
        std::array<uint8_t, TAG_SIZE> tag{};

        // Encrypt
        CipherCtxGuard ctxGuard;
        if (!ctxGuard.valid()) {
            return tl::unexpected(Error{"Failed to create cipher context"});
        }
        EVP_CIPHER_CTX* ctx = ctxGuard.ctx;

        if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
            return tl::unexpected(Error{"EVP_EncryptInit_ex (alg): " + opensslErrors()});
        }
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, static_cast<int>(IV_SIZE), nullptr) != 1) {
            return tl::unexpected(Error{"EVP_CTRL_GCM_SET_IVLEN: " + opensslErrors()});
        }
        if (EVP_EncryptInit_ex(ctx, nullptr, nullptr, keyBytes.data(), iv.data()) != 1) {
            return tl::unexpected(Error{"EVP_EncryptInit_ex (key/iv): " + opensslErrors()});
        }

        int outLen = 0;
        if (EVP_EncryptUpdate(ctx,
                ciphertext.data(), &outLen,
                reinterpret_cast<const uint8_t*>(plaintext.data()),
                static_cast<int>(plaintext.size())) != 1)
        {
            return tl::unexpected(Error{"EVP_EncryptUpdate: " + opensslErrors()});
        }

        int finalLen = 0;
        if (EVP_EncryptFinal_ex(ctx, ciphertext.data() + outLen, &finalLen) != 1) {
            return tl::unexpected(Error{"EVP_EncryptFinal_ex: " + opensslErrors()});
        }
        ciphertext.resize(static_cast<size_t>(outLen + finalLen));

        // Extract GCM authentication tag
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, static_cast<int>(TAG_SIZE), tag.data()) != 1) {
            return tl::unexpected(Error{"EVP_CTRL_GCM_GET_TAG: " + opensslErrors()});
        }

        // Format: base64(iv) . base64(tag) . base64(ciphertext)
        std::string encoded;
        encoded.reserve(64 + 32 + ((plaintext.size() * 4 / 3) + 4));
        encoded += base64Encode(std::span<const uint8_t>(iv.data(), iv.size()));
        encoded += BLOB_SEPARATOR;
        encoded += base64Encode(std::span<const uint8_t>(tag.data(), tag.size()));
        encoded += BLOB_SEPARATOR;
        encoded += base64Encode(std::span<const uint8_t>(ciphertext.data(), ciphertext.size()));

        return encoded;
    }

    // -----------------------------------------------------------------------------
    // AES-256-GCM decrypt
    // -----------------------------------------------------------------------------
    CryptoUtils::Result<std::string> CryptoUtils::aesDecrypt(
        std::string_view encoded,
        std::span<const uint8_t> keyBytes)
    {
        if (keyBytes.size() != 32) {
            return tl::unexpected(Error{"AES-256 requires exactly 32 key bytes"});
        }

        // Split blob
        const auto parts = splitString(encoded, BLOB_SEPARATOR, 3);
        if (parts.size() != 3) {
            return tl::unexpected(Error{"Invalid encrypted blob format: expected iv.tag.ciphertext"});
        }

        auto ivResult         = base64Decode(parts[0]);
        auto tagResult        = base64Decode(parts[1]);
        auto ciphertextResult = base64Decode(parts[2]);

        if (!ivResult)         return tl::unexpected(ivResult.error());
        if (!tagResult)        return tl::unexpected(tagResult.error());
        if (!ciphertextResult) return tl::unexpected(ciphertextResult.error());

        const auto& iv         = *ivResult;
        auto&       tag        = *tagResult;      // non-const: EVP_CTRL_GCM_SET_TAG takes void*
        const auto& ciphertext = *ciphertextResult;

        if (iv.size() != IV_SIZE) {
            return tl::unexpected(Error{"IV length mismatch"});
        }
        if (tag.size() != TAG_SIZE) {
            return tl::unexpected(Error{"Tag length mismatch"});
        }

        // Decrypt
        CipherCtxGuard ctxGuard;
        if (!ctxGuard.valid()) {
            return tl::unexpected(Error{"Failed to create cipher context"});
        }
        EVP_CIPHER_CTX* ctx = ctxGuard.ctx;

        if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
            return tl::unexpected(Error{"EVP_DecryptInit_ex (alg): " + opensslErrors()});
        }
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, static_cast<int>(IV_SIZE), nullptr) != 1) {
            return tl::unexpected(Error{"EVP_CTRL_GCM_SET_IVLEN: " + opensslErrors()});
        }
        if (EVP_DecryptInit_ex(ctx, nullptr, nullptr, keyBytes.data(), iv.data()) != 1) {
            return tl::unexpected(Error{"EVP_DecryptInit_ex (key/iv): " + opensslErrors()});
        }

        std::vector<uint8_t> plaintext(ciphertext.size());
        int outLen = 0;
        if (EVP_DecryptUpdate(ctx,
                plaintext.data(), &outLen,
                ciphertext.data(),
                static_cast<int>(ciphertext.size())) != 1)
        {
            return tl::unexpected(Error{"EVP_DecryptUpdate: " + opensslErrors()});
        }

        // Set expected tag before calling Final
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG,
                static_cast<int>(TAG_SIZE), tag.data()) != 1)
        {
            return tl::unexpected(Error{"EVP_CTRL_GCM_SET_TAG: " + opensslErrors()});
        }

        int finalLen = 0;
        // EVP_DecryptFinal_ex returns <= 0 if tag verification fails
        if (EVP_DecryptFinal_ex(ctx, plaintext.data() + outLen, &finalLen) <= 0) {
            // Don't leak OpenSSL details on auth failure - could be a timing oracle
            return tl::unexpected(Error{
                "Decryption failed: authentication tag mismatch. "
                "Data may be corrupt or the master key has changed."
            });
        }
        plaintext.resize(static_cast<size_t>(outLen + finalLen));

        return std::string(reinterpret_cast<const char*>(plaintext.data()), plaintext.size());
    }

    // -----------------------------------------------------------------------------
    // Certificate utilities
    // -----------------------------------------------------------------------------
    CryptoUtils::Result<std::string> CryptoUtils::certFingerprint(std::string_view pemCert) {
        BioGuard bio(BIO_new_mem_buf(pemCert.data(), static_cast<int>(pemCert.size())));
        if (!bio.bio) {
            return tl::unexpected(Error{"Failed to create BIO for certificate"});
        }

        X509Guard cert(PEM_read_bio_X509(bio.bio, nullptr, nullptr, nullptr));
        if (!cert.cert) {
            return tl::unexpected(Error{"Failed to parse PEM certificate: " + opensslErrors()});
        }

        std::array<uint8_t, EVP_MAX_MD_SIZE> digest{};
        unsigned int digestLen = 0;
        if (X509_digest(cert.cert, EVP_sha256(), digest.data(), &digestLen) != 1) {
            return tl::unexpected(Error{"X509_digest failed: " + opensslErrors()});
        }

        // Format as colon-separated hex: "aa:bb:cc:..."
        std::ostringstream oss;
        oss << std::hex << std::setfill('0');
        for (unsigned int i = 0; i < digestLen; ++i) {
            if (i > 0) oss << ':';
            oss << std::setw(2) << static_cast<unsigned>(digest[i]);
        }
        return oss.str();
    }

    CryptoUtils::Result<std::string> CryptoUtils::certSubjectUID(std::string_view pemCert) {
        BioGuard bio(BIO_new_mem_buf(pemCert.data(), static_cast<int>(pemCert.size())));
        if (!bio.bio) {
            return tl::unexpected(Error{"Failed to create BIO"});
        }

        X509Guard cert(PEM_read_bio_X509(bio.bio, nullptr, nullptr, nullptr));
        if (!cert.cert) {
            return tl::unexpected(Error{"Failed to parse PEM certificate: " + opensslErrors()});
        }

        X509_NAME* subject = X509_get_subject_name(cert.cert);
        if (!subject) {
            return tl::unexpected(Error{"Certificate has no subject name"});
        }

        // One-line subject string, e.g. "CN=SMP_example, O=Example Corp, C=DE"
        BioGuard subjectBio(BIO_new(BIO_s_mem()));
        X509_NAME_print_ex(subjectBio.bio, subject, 0, XN_FLAG_RFC2253);

        BUF_MEM* bptr;
        BIO_get_mem_ptr(subjectBio.bio, &bptr);
        return std::string(bptr->data, bptr->length);
    }

    CryptoUtils::Result<CryptoUtils::CertValidity> CryptoUtils::certValidity(std::string_view pemCert) {
        BioGuard bio(BIO_new_mem_buf(pemCert.data(), static_cast<int>(pemCert.size())));
        if (!bio.bio) {
            return tl::unexpected(Error{"Failed to create BIO"});
        }

        X509Guard cert(PEM_read_bio_X509(bio.bio, nullptr, nullptr, nullptr));
        if (!cert.cert) {
            return tl::unexpected(Error{"Failed to parse PEM certificate: " + opensslErrors()});
        }

        // Format ASN1_TIME as ISO8601
        auto asn1ToIso = [](const ASN1_TIME* t) -> std::string {
            BIO* mem = BIO_new(BIO_s_mem());
            ASN1_TIME_print(mem, t);
            BUF_MEM* bptr;
            BIO_get_mem_ptr(mem, &bptr);
            std::string result(bptr->data, bptr->length);
            BIO_free(mem);
            return result;
        };

        CertValidity validity;
        validity.notBefore = asn1ToIso(X509_get0_notBefore(cert.cert));
        validity.notAfter  = asn1ToIso(X509_get0_notAfter(cert.cert));
        return validity;
    }

    CryptoUtils::Result<void> CryptoUtils::verifyCertKeyPair(
        std::string_view pemCert,
        std::string_view pemPrivateKey)
    {
        // Load cert
        BioGuard certBio(BIO_new_mem_buf(pemCert.data(), static_cast<int>(pemCert.size())));
        X509Guard cert(PEM_read_bio_X509(certBio.bio, nullptr, nullptr, nullptr));
        if (!cert.cert) {
            return tl::unexpected(Error{"Failed to parse certificate: " + opensslErrors()});
        }

        // Load private key
        BioGuard keyBio(BIO_new_mem_buf(pemPrivateKey.data(), static_cast<int>(pemPrivateKey.size())));
        PkeyGuard pkey(PEM_read_bio_PrivateKey(keyBio.bio, nullptr, nullptr, nullptr));
        if (!pkey.key) {
            return tl::unexpected(Error{"Failed to parse private key: " + opensslErrors()});
        }

        // X509_check_private_key: returns 1 if they match
        if (X509_check_private_key(cert.cert, pkey.key) != 1) {
            return tl::unexpected(Error{
                "Certificate and private key do not match: " + opensslErrors()
            });
        }

        return {}; // void success
    }


    // -----------------------------------------------------------------------------
    // bcrypt password hashing
    //
    // Thread safety: BCrypt::generateHash / validatePassword in crypt_blowfish.c
    // operate entirely on stack-allocated state with no global mutable writes
    // during hashing. Concurrent calls from multiple threads are safe.
    //
    // The 72-byte limit is a hard bcrypt algorithm constraint. Passwords longer
    // than 72 bytes are silently truncated by the underlying implementation.
    // We enforce it explicitly and return an error rather than silently truncate.
    //
    // CPU cost: ~100ms at workFactor=12 on modern hardware. NEVER call these
    // on a Drogon IO thread. Offload via drogon::async_run().
    // -----------------------------------------------------------------------------
    CryptoUtils::Result<std::string> CryptoUtils::bcryptHash(
        std::string_view password,
        int workFactor)
    {
        if (password.empty())
            return tl::unexpected(Error{"Password must not be empty"});

        if (password.size() > BCRYPT_MAX_PASSWORD_LEN)
            return tl::unexpected(Error{
                "Password exceeds bcrypt maximum of 72 bytes"
            });

        if (workFactor < 4 || workFactor > 31)
            return tl::unexpected(Error{
                "bcrypt work factor must be between 4 and 31"
            });

        try {
            std::string hash = BCrypt::generateHash(std::string(password), workFactor);
            if (hash.empty())
                return tl::unexpected(Error{
                    "bcrypt hash generation returned empty string"
                });
            return hash;
        } catch (const std::exception& e) {
            return tl::unexpected(Error{
                std::string("bcrypt hash failed: ") + e.what()
            });
        }
    }

    CryptoUtils::Result<bool> CryptoUtils::bcryptVerify(
        std::string_view password,
        std::string_view hash)
    {
        if (password.empty())
            return tl::unexpected(Error{"Password must not be empty"});

        // Validate hash prefix - bcrypt hashes always start with $2a$ or $2b$
        if (hash.size() < 4 ||
            (hash.substr(0, 4) != "$2b$" && hash.substr(0, 4) != "$2a$"))
        {
            return tl::unexpected(Error{
                "Invalid bcrypt hash format: expected $2b$ or $2a$ prefix"
            });
        }

        try {
            bool match = BCrypt::validatePassword(
                std::string(password),
                std::string(hash)
            );
            return match;
        } catch (const std::exception& e) {
            // validatePassword throws on malformed hash, not on mismatch
            return tl::unexpected(Error{
                std::string("bcrypt verify failed: ") + e.what()
            });
        }
    }

} // namespace SMPPlusPlus