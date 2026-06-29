//
// Created by Saadat Baig on 29.06.26.
//
#include "certificate_repository.h"


namespace SMPPlusPlus
{
    namespace {

        SmpCertificate rowToCert(const drogon::orm::Row& row) {
            SmpCertificate c;
            c.id            = row["id"].as<std::string>();
            c.label         = row["label"].as<std::string>();
            c.certPem       = row["cert_pem"].as<std::string>();
            c.privateKeyEnc = row["private_key_enc"].as<std::string>();
            c.fingerprint   = row["fingerprint"].as<std::string>();
            c.isActive      = row["is_active"].as<bool>();
            c.createdAt     = row["created_at"].as<std::string>();
            if (!row["valid_from"].isNull())
                c.validFrom = row["valid_from"].as<std::string>();
            if (!row["valid_until"].isNull())
                c.validUntil = row["valid_until"].as<std::string>();
            if (!row["created_by"].isNull())
                c.createdBy = row["created_by"].as<std::string>();
            return c;
        }

    } // anonymous namespace

    drogon::Task<std::vector<SmpCertificate>> CertificateRepository::findAll() {
        auto db = drogon::app().getFastDbClient("default");
        std::vector<SmpCertificate> results;

        try {
            auto rows = co_await db->execSqlCoro(
                "SELECT id, label, cert_pem, private_key_enc, fingerprint, "
                "valid_from, valid_until, is_active, created_at, created_by "
                "FROM smp_certificates ORDER BY created_at DESC"
            );
            for (const auto& row : rows)
                results.push_back(rowToCert(row));
        } catch (const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "[CertificateRepository] findAll: " << e.base().what();
        }
        co_return results;
    }

    drogon::Task<std::optional<SmpCertificate>> CertificateRepository::findById(
        const std::string& id)
    {
        auto db = drogon::app().getFastDbClient("default");

        try {
            auto rows = co_await db->execSqlCoro(
                "SELECT id, label, cert_pem, private_key_enc, fingerprint, "
                "valid_from, valid_until, is_active, created_at, created_by "
                "FROM smp_certificates WHERE id = $1 LIMIT 1",
                id
            );
            if (!rows.empty()) co_return rowToCert(rows[0]);
        } catch (const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "[CertificateRepository] findById: " << e.base().what();
        }
        co_return std::nullopt;
    }

    drogon::Task<std::optional<SmpCertificate>> CertificateRepository::findActive() {
        auto db = drogon::app().getFastDbClient("default");

        try {
            auto rows = co_await db->execSqlCoro(
                "SELECT id, label, cert_pem, private_key_enc, fingerprint, "
                "valid_from, valid_until, is_active, created_at, created_by "
                "FROM smp_certificates WHERE is_active = true LIMIT 1"
            );
            if (!rows.empty()) co_return rowToCert(rows[0]);
        } catch (const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "[CertificateRepository] findActive: " << e.base().what();
        }
        co_return std::nullopt;
    }

    drogon::Task<std::string> CertificateRepository::create(
        const std::string& label,
        const std::string& certPem,
        const std::string& privateKeyEnc,
        const std::string& fingerprint,
        const std::string& validFrom,
        const std::string& validUntil,
        const std::string& createdByUserId)
    {
        auto db = drogon::app().getFastDbClient("default");

        try {
            auto rows = co_await db->execSqlCoro(
                "INSERT INTO smp_certificates "
                "(label, cert_pem, private_key_enc, fingerprint, "
                "valid_from, valid_until, created_by) "
                "VALUES ($1, $2, $3, $4, "
                "NULLIF($5, '')::TIMESTAMPTZ, NULLIF($6, '')::TIMESTAMPTZ, $7) "
                "RETURNING id",
                label, certPem, privateKeyEnc, fingerprint,
                validFrom, validUntil, createdByUserId
            );
            if (!rows.empty()) co_return rows[0]["id"].as<std::string>();
        } catch (const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "[CertificateRepository] create: " << e.base().what();
            throw;
        }
        co_return std::string{};
    }

    drogon::Task<bool> CertificateRepository::activate(const std::string& id) {
        auto db = drogon::app().getFastDbClient("default");

        try {
            // Atomic: deactivate all, then activate the target
            // The partial unique index enforces only one active at a time
            co_await db->execSqlCoro(
                "UPDATE smp_certificates SET is_active = false "
                "WHERE is_active = true"
            );
            auto result = co_await db->execSqlCoro(
                "UPDATE smp_certificates SET is_active = true WHERE id = $1",
                id
            );
            co_return result.affectedRows() > 0;
        } catch (const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "[CertificateRepository] activate: " << e.base().what();
            co_return false;
        }
    }

    drogon::Task<bool> CertificateRepository::remove(const std::string& id) {
        auto db = drogon::app().getFastDbClient("default");

        try {
            auto result = co_await db->execSqlCoro(
                "DELETE FROM smp_certificates WHERE id = $1 AND is_active = false",
                id
            );
            co_return result.affectedRows() > 0;
        } catch (const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "[CertificateRepository] remove: " << e.base().what();
            co_return false;
        }
    }

} // namespace SMPPlusPlus
