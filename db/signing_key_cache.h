//
// Created by Saadat Baig on 29.06.26.
//
#pragma once

#include <memory>
#include <shared_mutex>
#include <string>
#include <optional>
#include <openssl/evp.h>

#include <drogon/drogon.h>


namespace SMPPlusPlus
{
    /**
     * SigningKeyCache - Thread-safe singleton cache for the active SMP signing key.
     *
     * The active SMP certificate's private key is expensive to decrypt on every
     * request (AES-256-GCM decrypt + PEM parse). We cache the parsed EVP_PKEY*
     * in memory and only reload when the active certificate changes.
     *
     * Threading model:
     *   - Many concurrent readers (signing requests): hold shared_lock
     *   - Rare writer (cert activation): holds unique_lock
     *   - Reader/writer access is fully concurrent via std::shared_mutex
     *
     * Lifecycle:
     *   - loadFromDb() called at startup (before app.run()) and on cert activation
     *   - get() called on every SignedServiceMetadata request
     *   - invalidate() called before reloading
     */
    class SigningKeyCache {
    public:
        static SigningKeyCache& instance();

        // Holds a reference to the cached key + cert for the duration of a sign op.
        // The shared_ptr keeps the key alive even if the cache is invalidated
        // mid-request during cert rotation.
        struct Entry {
            std::shared_ptr<EVP_PKEY> key;  // custom deleter calls EVP_PKEY_free
            std::string certPem;            // public cert for embedding in KeyInfo
            std::string certId;             // for cache invalidation checks
        };

        /**
         * Load the active certificate from the DB and cache the decrypted key.
         * Safe to call before app.run() (uses libpq directly) and after (uses
         * Drogon's async client via coroutine).
         *
         * @param connString  libpq connection string (used for startup load only)
         */
        void loadFromDb(const std::string& connString);

        void loadEntry(const std::string& certId, const std::string& certPem, const std::string& encryptedKey);

        /**
         * Async reload — call this from CertificateController::activate()
         * after a new cert is activated. Returns a coroutine task.
         */
        drogon::Task<void> reloadAsync();

        /**
         * Get the currently cached signing key entry.
         * Returns nullopt if no active certificate is configured.
         *
         * The returned shared_ptr keeps the key alive even during cert rotation.
         * Call sites must not hold this beyond the current request.
         */
        std::optional<Entry> get() const;

        /// True if a signing key is loaded and ready
        bool ready() const;

        /// Invalidate the cache (called before reload)
        void invalidate();

    private:
        SigningKeyCache() = default;
        SigningKeyCache(const SigningKeyCache&) = delete;
        SigningKeyCache& operator=(const SigningKeyCache&) = delete;

        mutable std::shared_mutex mutex_;
        std::optional<Entry> entry_;
    };

} // namespace SMPPlusPlus
