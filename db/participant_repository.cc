//
// Created by Saadat Baig on 29.06.26.
//
#include "participant_repository.h"


namespace SMPPlusPlus
{
    namespace {

        Participant rowToParticipant(const drogon::orm::Row& row) {
            Participant p;
            p.id                = row["id"].as<std::string>();
            p.tenantId          = row["tenant_id"].as<std::string>();
            p.identifierScheme  = row["identifier_scheme"].as<std::string>();
            p.identifierValue   = row["identifier_value"].as<std::string>();
            p.createdAt         = row["created_at"].as<std::string>();
            if (!row["created_by"].isNull())
            {
                p.createdBy = row["created_by"].as<std::string>();
            }

            return p;
        }

        Endpoint rowToEndpoint(const drogon::orm::Row& row) {
            Endpoint e;
            e.id                      = row["id"].as<std::string>();
            e.processId               = row["process_id"].as<std::string>();
            e.transportProfile        = row["transport_profile"].as<std::string>();
            e.endpointUrl             = row["endpoint_url"].as<std::string>();
            e.apCertificatePem        = row["ap_certificate_pem"].as<std::string>();
            e.requireBls              = row["require_bls"].as<bool>();
            e.createdAt               = row["created_at"].as<std::string>();
            if (!row["service_activation_date"].isNull())
            {
                e.serviceActivationDate = row["service_activation_date"].as<std::string>();
            }
            if (!row["service_expiration_date"].isNull())
            {
                e.serviceExpirationDate = row["service_expiration_date"].as<std::string>();
            }
            if (!row["technical_contact_url"].isNull())
            {
                e.technicalContactUrl = row["technical_contact_url"].as<std::string>();
            }
            if (!row["technical_information_url"].isNull())
            {
                e.technicalInformationUrl = row["technical_information_url"].as<std::string>();
            }
            if (!row["service_description"].isNull())
            {
                e.serviceDescription = row["service_description"].as<std::string>();
            }

            return e;
        }

        Process rowToProcess(const drogon::orm::Row& row) {
            Process p;
            p.id                = row["id"].as<std::string>();
            p.serviceMetadataId = row["service_metadata_id"].as<std::string>();
            p.processScheme     = row["process_scheme"].as<std::string>();
            p.processValue      = row["process_value"].as<std::string>();
            p.createdAt         = row["created_at"].as<std::string>();

            return p;
        }

        ServiceMetadata rowToServiceMetadata(const drogon::orm::Row& row) {
            ServiceMetadata s;
            s.id                 = row["id"].as<std::string>();
            s.participantId      = row["participant_id"].as<std::string>();
            s.documentTypeScheme = row["document_type_scheme"].as<std::string>();
            s.documentTypeValue  = row["document_type_value"].as<std::string>();
            s.createdAt          = row["created_at"].as<std::string>();

            return s;
        }

    } // anonymous namespace

    // =============================================================================
    // Participants
    // =============================================================================

    drogon::Task<std::vector<Participant>> ParticipantRepository::findByTenantId(
        const std::string& tenantId)
    {
        auto db = drogon::app().getFastDbClient("default");
        std::vector<Participant> results;

        try
        {
            auto rows = co_await db->execSqlCoro(
                "SELECT id, tenant_id, identifier_scheme, identifier_value, "
                "created_at, created_by FROM participants "
                "WHERE tenant_id = $1 ORDER BY identifier_value ASC",
                tenantId
            );
            for (const auto& row : rows)
                results.push_back(rowToParticipant(row));
        } catch (const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "[ParticipantRepository] findByTenantId: " << e.base().what();
        }
        co_return results;
    }

    drogon::Task<std::optional<Participant>> ParticipantRepository::findById(
        const std::string& id,
        const std::string& tenantId)
    {
        auto db = drogon::app().getFastDbClient("default");

        try {
            auto rows = co_await db->execSqlCoro(
                "SELECT id, tenant_id, identifier_scheme, identifier_value, "
                "created_at, created_by FROM participants "
                "WHERE id = $1 AND tenant_id = $2 LIMIT 1",
                id, tenantId
            );
            if (!rows.empty()) co_return rowToParticipant(rows[0]);
        } catch (const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "[ParticipantRepository] findById: " << e.base().what();
        }
        co_return std::nullopt;
    }

    drogon::Task<bool> ParticipantRepository::existsByIdentifier(
        const std::string& tenantId,
        const std::string& scheme,
        const std::string& value)
    {
        auto db = drogon::app().getFastDbClient("default");

        try {
            auto rows = co_await db->execSqlCoro(
                "SELECT 1 FROM participants "
                "WHERE tenant_id = $1 AND identifier_scheme = $2 "
                "AND identifier_value = $3 LIMIT 1",
                tenantId, scheme, value
            );
            co_return !rows.empty();
        } catch (const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "[ParticipantRepository] existsByIdentifier: " << e.base().what();
            co_return false;
        }
    }

    drogon::Task<std::string> ParticipantRepository::create(
        const std::string& tenantId,
        const std::string& scheme,
        const std::string& value,
        const std::string& createdByUserId)
    {
        auto db = drogon::app().getFastDbClient("default");

        try {
            auto rows = co_await db->execSqlCoro(
                "INSERT INTO participants "
                "(tenant_id, identifier_scheme, identifier_value, created_by) "
                "VALUES ($1, $2, $3, $4) RETURNING id",
                tenantId, scheme, value, createdByUserId
            );
            if (!rows.empty()) co_return rows[0]["id"].as<std::string>();
        } catch (const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "[ParticipantRepository] create: " << e.base().what();
            throw;
        }
        co_return std::string{};
    }

    drogon::Task<bool> ParticipantRepository::remove(
        const std::string& id,
        const std::string& tenantId)
    {
        auto db = drogon::app().getFastDbClient("default");

        try {
            auto result = co_await db->execSqlCoro(
                "DELETE FROM participants WHERE id = $1 AND tenant_id = $2",
                id, tenantId
            );
            co_return result.affectedRows() > 0;
        } catch (const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "[ParticipantRepository] remove: " << e.base().what();
            co_return false;
        }
    }

    // =============================================================================
    // findPagedByTenantId
    // =============================================================================
    drogon::Task<ParticipantRepository::PagedParticipants> ParticipantRepository::findPagedByTenantId(
        const std::string& tenantId,
        int                limit,
        int                offset,
        const std::string& orderBy,
        const std::string& orderDirection,
        const std::string& search)
    {
        auto db = drogon::app().getFastDbClient("default");
        PagedParticipants result;

        // orderBy and orderDirection are whitelisted upstream via
        // Pagination::fromRequest. String-interpolating them here is safe
        // BECAUSE of that whitelist - never do it with untrusted input.
        const std::string safeOrderBy   = orderBy.empty()
                                              ? "identifier_value"
                                              : orderBy;
        const std::string safeDirection = (orderDirection == "DESC")
                                              ? "DESC"
                                              : "ASC";

        try {
            // Count query - runs against the same filter set as the data query.
            // Passing search as $2 means an empty string becomes "%%" which
            // matches everything - PostgreSQL treats this correctly.
            auto countRows = co_await db->execSqlCoro(
                "SELECT COUNT(*) AS total FROM participants "
                "WHERE tenant_id = $1 "
                "AND ("
                "  $2 = '' OR "
                "  identifier_value  ILIKE '%' || $2 || '%' OR "
                "  identifier_scheme ILIKE '%' || $2 || '%'"
                ")",
                tenantId, search
            );
            result.total = countRows[0]["total"].as<int>();

            // Data query
            const std::string dataSql =
                "SELECT id, tenant_id, identifier_scheme, identifier_value, "
                "created_at, created_by "
                "FROM participants "
                "WHERE tenant_id = $1 "
                "AND ("
                "  $2 = '' OR "
                "  identifier_value  ILIKE '%' || $2 || '%' OR "
                "  identifier_scheme ILIKE '%' || $2 || '%'"
                ") "
                "ORDER BY " + safeOrderBy + " " + safeDirection + ", id ASC "
                "LIMIT $3::int OFFSET $4::int";

            auto rows = co_await db->execSqlCoro(
                dataSql, tenantId, search, limit, offset
            );

            for (const auto& row : rows) {
                Participant p;
                p.id                = row["id"].as<std::string>();
                p.tenantId          = row["tenant_id"].as<std::string>();
                p.identifierScheme  = row["identifier_scheme"].as<std::string>();
                p.identifierValue   = row["identifier_value"].as<std::string>();
                p.createdAt         = row["created_at"].as<std::string>();
                if (!row["created_by"].isNull())
                    p.createdBy = row["created_by"].as<std::string>();
                result.items.push_back(std::move(p));
            }

        } catch (const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "[ParticipantRepository] findPagedByTenantId: "
                      << e.base().what();
        }

        co_return result;
    }

    // =============================================================================
    // ServiceMetadata
    // =============================================================================

    drogon::Task<std::vector<ServiceMetadata>> ParticipantRepository::findServicesByParticipantId(
        const std::string& participantId,
        const std::string& tenantId)
    {
        auto db = drogon::app().getFastDbClient("default");
        std::vector<ServiceMetadata> results;

        try {
            auto check = co_await db->execSqlCoro(
                "SELECT 1 FROM participants WHERE id = $1 AND tenant_id = $2 LIMIT 1",
                participantId, tenantId
            );
            if (check.empty()) co_return results;

            auto smRows = co_await db->execSqlCoro(
                "SELECT id, participant_id, document_type_scheme, document_type_value, "
                "created_at FROM service_metadata WHERE participant_id = $1 "
                "ORDER BY document_type_value ASC",
                participantId
            );

            for (const auto& smRow : smRows) {
                ServiceMetadata sm = rowToServiceMetadata(smRow);

                auto procRows = co_await db->execSqlCoro(
                    "SELECT id, service_metadata_id, process_scheme, process_value, "
                    "created_at FROM processes WHERE service_metadata_id = $1 "
                    "ORDER BY process_value ASC",
                    sm.id
                );

                for (const auto& procRow : procRows) {
                    Process proc = rowToProcess(procRow);

                    auto epRows = co_await db->execSqlCoro(
                        "SELECT id, process_id, transport_profile, endpoint_url, "
                        "ap_certificate_pem, require_bls, service_activation_date, "
                        "service_expiration_date, technical_contact_url, "
                        "technical_information_url, service_description, created_at "
                        "FROM endpoints WHERE process_id = $1",
                        proc.id
                    );

                    for (const auto& epRow : epRows)
                        proc.endpoints.push_back(rowToEndpoint(epRow));

                    sm.processes.push_back(std::move(proc));
                }

                results.push_back(std::move(sm));
            }
        } catch (const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "[ParticipantRepository] findServicesByParticipantId: "
                      << e.base().what();
        }
        co_return results;
    }

    drogon::Task<std::optional<ServiceMetadata>> ParticipantRepository::findServiceById(
        const std::string& serviceId,
        const std::string& participantId)
    {
        auto db = drogon::app().getFastDbClient("default");

        try {
            auto rows = co_await db->execSqlCoro(
                "SELECT id, participant_id, document_type_scheme, document_type_value, "
                "created_at FROM service_metadata "
                "WHERE id = $1 AND participant_id = $2 LIMIT 1",
                serviceId, participantId
            );
            if (rows.empty()) co_return std::nullopt;

            ServiceMetadata sm = rowToServiceMetadata(rows[0]);

            auto procRows = co_await db->execSqlCoro(
                "SELECT id, service_metadata_id, process_scheme, process_value, "
                "created_at FROM processes WHERE service_metadata_id = $1",
                sm.id
            );

            for (const auto& procRow : procRows) {
                Process proc = rowToProcess(procRow);

                auto epRows = co_await db->execSqlCoro(
                    "SELECT id, process_id, transport_profile, endpoint_url, "
                    "ap_certificate_pem, require_bls, service_activation_date, "
                    "service_expiration_date, technical_contact_url, "
                    "technical_information_url, service_description, created_at "
                    "FROM endpoints WHERE process_id = $1",
                    proc.id
                );
                for (const auto& epRow : epRows)
                    proc.endpoints.push_back(rowToEndpoint(epRow));

                sm.processes.push_back(std::move(proc));
            }

            co_return sm;
        } catch (const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "[ParticipantRepository] findServiceById: " << e.base().what();
            co_return std::nullopt;
        }
    }

    drogon::Task<bool> ParticipantRepository::serviceExistsByDocType(
        const std::string& participantId,
        const std::string& docTypeScheme,
        const std::string& docTypeValue)
    {
        auto db = drogon::app().getFastDbClient("default");

        try {
            auto rows = co_await db->execSqlCoro(
                "SELECT 1 FROM service_metadata "
                "WHERE participant_id = $1 AND document_type_scheme = $2 "
                "AND document_type_value = $3 LIMIT 1",
                participantId, docTypeScheme, docTypeValue
            );
            co_return !rows.empty();
        } catch (const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "[ParticipantRepository] serviceExistsByDocType: " << e.base().what();
            co_return false;
        }
    }

    drogon::Task<std::string> ParticipantRepository::createService(
        const std::string& participantId,
        const std::string& docTypeScheme,
        const std::string& docTypeValue)
    {
        auto db = drogon::app().getFastDbClient("default");

        try {
            auto rows = co_await db->execSqlCoro(
                "INSERT INTO service_metadata "
                "(participant_id, document_type_scheme, document_type_value) "
                "VALUES ($1, $2, $3) RETURNING id",
                participantId, docTypeScheme, docTypeValue
            );
            if (!rows.empty()) co_return rows[0]["id"].as<std::string>();
        } catch (const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "[ParticipantRepository] createService: " << e.base().what();
            throw;
        }
        co_return std::string{};
    }

    drogon::Task<bool> ParticipantRepository::removeService(
        const std::string& serviceId,
        const std::string& participantId)
    {
        auto db = drogon::app().getFastDbClient("default");

        try {
            auto result = co_await db->execSqlCoro(
                "DELETE FROM service_metadata WHERE id = $1 AND participant_id = $2",
                serviceId, participantId
            );
            co_return result.affectedRows() > 0;
        } catch (const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "[ParticipantRepository] removeService: " << e.base().what();
            co_return false;
        }
    }

    // =============================================================================
    // Processes
    // =============================================================================

    drogon::Task<std::string> ParticipantRepository::createProcess(
        const std::string& serviceMetadataId,
        const std::string& processScheme,
        const std::string& processValue)
    {
        auto db = drogon::app().getFastDbClient("default");

        try {
            auto rows = co_await db->execSqlCoro(
                "INSERT INTO processes (service_metadata_id, process_scheme, process_value) "
                "VALUES ($1, $2, $3) RETURNING id",
                serviceMetadataId, processScheme, processValue
            );
            if (!rows.empty()) co_return rows[0]["id"].as<std::string>();
        } catch (const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "[ParticipantRepository] createProcess: " << e.base().what();
            throw;
        }
        co_return std::string{};
    }

    drogon::Task<bool> ParticipantRepository::removeProcess(
        const std::string& processId,
        const std::string& serviceMetadataId)
    {
        auto db = drogon::app().getFastDbClient("default");

        try {
            auto result = co_await db->execSqlCoro(
                "DELETE FROM processes WHERE id = $1 AND service_metadata_id = $2",
                processId, serviceMetadataId
            );
            co_return result.affectedRows() > 0;
        } catch (const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "[ParticipantRepository] removeProcess: " << e.base().what();
            co_return false;
        }
    }

    // =============================================================================
    // Endpoints
    // =============================================================================

    drogon::Task<std::string> ParticipantRepository::createEndpoint(
        const std::string& processId,
        const std::string& transportProfile,
        const std::string& endpointUrl,
        const std::string& apCertificatePem,
        bool               requireBls,
        const std::string& serviceActivationDate,
        const std::string& serviceExpirationDate,
        const std::string& technicalContactUrl,
        const std::string& technicalInformationUrl,
        const std::string& serviceDescription)
    {
        auto db = drogon::app().getFastDbClient("default");

        try {
            auto rows = co_await db->execSqlCoro(
                "INSERT INTO endpoints (process_id, transport_profile, endpoint_url, "
                "ap_certificate_pem, require_bls, service_activation_date, "
                "service_expiration_date, technical_contact_url, "
                "technical_information_url, service_description) "
                "VALUES ($1, $2, $3, $4, $5, "
                "NULLIF($6, '')::TIMESTAMPTZ, NULLIF($7, '')::TIMESTAMPTZ, "
                "NULLIF($8, ''), NULLIF($9, ''), NULLIF($10, '')) "
                "RETURNING id",
                processId, transportProfile, endpointUrl, apCertificatePem,
                requireBls, serviceActivationDate, serviceExpirationDate,
                technicalContactUrl, technicalInformationUrl, serviceDescription
            );
            if (!rows.empty()) co_return rows[0]["id"].as<std::string>();
        } catch (const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "[ParticipantRepository] createEndpoint: " << e.base().what();
            throw;
        }
        co_return std::string{};
    }

    drogon::Task<bool> ParticipantRepository::removeEndpoint(
        const std::string& endpointId,
        const std::string& processId)
    {
        auto db = drogon::app().getFastDbClient("default");

        try {
            auto result = co_await db->execSqlCoro(
                "DELETE FROM endpoints WHERE id = $1 AND process_id = $2",
                endpointId, processId
            );
            co_return result.affectedRows() > 0;
        } catch (const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "[ParticipantRepository] removeEndpoint: " << e.base().what();
            co_return false;
        }
    }

    drogon::Task<std::optional<Endpoint>> ParticipantRepository::findEndpointById(
    const std::string& endpointId,
    const std::string& processId)
    {
        auto db = drogon::app().getFastDbClient("default");

        try {
            auto rows = co_await db->execSqlCoro(
                "SELECT id, process_id, transport_profile, endpoint_url, "
                "ap_certificate_pem, require_bls, service_activation_date, "
                "service_expiration_date, technical_contact_url, "
                "technical_information_url, service_description, created_at "
                "FROM endpoints "
                "WHERE id = $1 AND process_id = $2 LIMIT 1",
                endpointId, processId
            );
            if (!rows.empty()) co_return rowToEndpoint(rows[0]);
        } catch (const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "[ParticipantRepository] findEndpointById: "
                      << e.base().what();
        }

        co_return std::nullopt;
    }

    drogon::Task<bool> ParticipantRepository::updateEndpoint(
        const std::string& endpointId,
        const std::string& processId,
        const std::string& transportProfile,
        const std::string& endpointUrl,
        const std::string& apCertificatePem,
        bool               requireBls,
        const std::string& serviceActivationDate,
        const std::string& serviceExpirationDate,
        const std::string& technicalContactUrl,
        const std::string& technicalInformationUrl,
        const std::string& serviceDescription)
    {
        auto db = drogon::app().getFastDbClient("default");

        try {
            auto result = co_await db->execSqlCoro(
                "UPDATE endpoints SET "
                "  transport_profile         = $3, "
                "  endpoint_url              = $4, "
                "  ap_certificate_pem        = $5, "
                "  require_bls               = $6, "
                "  service_activation_date   = NULLIF($7,  '')::TIMESTAMPTZ, "
                "  service_expiration_date   = NULLIF($8,  '')::TIMESTAMPTZ, "
                "  technical_contact_url     = NULLIF($9,  ''), "
                "  technical_information_url = NULLIF($10, ''), "
                "  service_description       = NULLIF($11, '') "
                "WHERE id = $1 AND process_id = $2",
                endpointId, processId,
                transportProfile, endpointUrl, apCertificatePem, requireBls,
                serviceActivationDate, serviceExpirationDate,
                technicalContactUrl, technicalInformationUrl, serviceDescription
            );
            co_return result.affectedRows() > 0;
        } catch (const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "[ParticipantRepository] updateEndpoint: "
                      << e.base().what();
            co_return false;
        }
    }

    // =============================================================================
    // One-shot complete service creation - atomic transaction
    // =============================================================================
    drogon::Task<std::optional<ServiceMetadata>> ParticipantRepository::createComplete(
        const std::string&            participantId,
        const CompleteServiceRequest& req)
    {
        auto db = drogon::app().getFastDbClient("default");
        std::string serviceId;

        try
        {
            {
                // Transaction commits automatically when trans goes out of scope
                // without an exception being thrown.
                auto trans = co_await db->newTransactionCoro();

                // 1. Check doc type uniqueness within the transaction
                auto dupCheck = co_await trans->execSqlCoro(
                    "SELECT 1 FROM service_metadata "
                    "WHERE participant_id = $1 "
                    "AND document_type_scheme = $2 "
                    "AND document_type_value = $3 LIMIT 1",
                    participantId, req.documentTypeScheme, req.documentTypeValue
                );
                if (!dupCheck.empty()) {
                    LOG_WARN << "[ParticipantRepository] createComplete: "
                                "duplicate document type for participant " << participantId;
                    co_return std::nullopt;
                }

                // 2. Insert service metadata
                auto smRows = co_await trans->execSqlCoro(
                    "INSERT INTO service_metadata "
                    "(participant_id, document_type_scheme, document_type_value) "
                    "VALUES ($1, $2, $3) RETURNING id",
                    participantId, req.documentTypeScheme, req.documentTypeValue
                );
                serviceId = smRows[0]["id"].as<std::string>();

                // 3. Insert process
                auto procRows = co_await trans->execSqlCoro(
                    "INSERT INTO processes "
                    "(service_metadata_id, process_scheme, process_value) "
                    "VALUES ($1, $2, $3) RETURNING id",
                    serviceId, req.processScheme, req.processValue
                );
                const std::string processId = procRows[0]["id"].as<std::string>();

                // 4. Insert endpoint
                co_await trans->execSqlCoro(
                    "INSERT INTO endpoints "
                    "(process_id, transport_profile, endpoint_url, ap_certificate_pem, "
                    "require_bls, service_activation_date, service_expiration_date, "
                    "technical_contact_url, technical_information_url, service_description) "
                    "VALUES ($1, $2, $3, $4, $5, "
                    "NULLIF($6, '')::TIMESTAMPTZ, NULLIF($7, '')::TIMESTAMPTZ, "
                    "NULLIF($8, ''), NULLIF($9, ''), NULLIF($10, ''))",
                    processId,
                    req.transportProfile,
                    req.endpointUrl,
                    req.apCertificatePem,
                    req.requireBls,
                    req.serviceActivationDate,
                    req.serviceExpirationDate,
                    req.technicalContactUrl,
                    req.technicalInformationUrl,
                    req.serviceDescription
                );
            }

            auto result = co_await findServiceById(serviceId, participantId);
            co_return result;
        } catch (const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "[ParticipantRepository] createComplete failed: "
                      << e.base().what();
            co_return std::nullopt;
        }
    }

} // namespace SMPPlusPlus
