//
// Created by Saadat Baig on 07.07.26.
//
#include "endpoint_audit_repository.h"


namespace SMPPlusPlus
{
    namespace {

        // Map a joined row directly - no separate mapping steps since this
        // is a flat denormalized shape used only by the audit list.
        EndpointAuditRow rowToAudit(const drogon::orm::Row& row) {
            EndpointAuditRow r;

            r.endpointId       = row["endpoint_id"].as<std::string>();
            r.endpointUrl      = row["endpoint_url"].as<std::string>();
            r.transportProfile = row["transport_profile"].as<std::string>();
            r.requireBls       = row["require_bls"].as<bool>();
            r.createdAt        = row["endpoint_created_at"].as<std::string>();

            if (!row["service_activation_date"].isNull())
                r.serviceActivationDate = row["service_activation_date"].as<std::string>();
            if (!row["service_expiration_date"].isNull())
                r.serviceExpirationDate = row["service_expiration_date"].as<std::string>();

            r.participantId    = row["participant_id"].as<std::string>();
            r.identifierScheme = row["identifier_scheme"].as<std::string>();
            r.identifierValue  = row["identifier_value"].as<std::string>();

            r.serviceMetadataId  = row["service_metadata_id"].as<std::string>();
            r.documentTypeScheme = row["document_type_scheme"].as<std::string>();
            r.documentTypeValue  = row["document_type_value"].as<std::string>();

            r.processId     = row["process_id"].as<std::string>();
            r.processScheme = row["process_scheme"].as<std::string>();
            r.processValue  = row["process_value"].as<std::string>();

            return r;
        }

        // Maps sort names from the whitelist to their fully-qualified column
        // references in the join. Table aliases in the query:
        //   e   - endpoints
        //   p   - participants
        //   sm  - service_metadata
        //   pr  - processes
        std::string qualifiedSortColumn(const std::string& orderBy) {
            if (orderBy == "identifier_value")     return "p.identifier_value";
            if (orderBy == "document_type_value")  return "sm.document_type_value";
            if (orderBy == "process_value")        return "pr.process_value";
            if (orderBy == "endpoint_url")         return "e.endpoint_url";
            if (orderBy == "created_at")           return "e.created_at";
            return "p.identifier_value"; // safe default
        }

    } // anonymous namespace

    drogon::Task<EndpointAuditRepository::PagedEndpoints>
    EndpointAuditRepository::findPagedByTenantId(
        const std::string& tenantId,
        int                limit,
        int                offset,
        const std::string& orderBy,
        const std::string& orderDirection,
        const std::string& search)
    {
        auto db = drogon::app().getFastDbClient("default");
        PagedEndpoints result;

        // Whitelist enforcement is done in Pagination::fromRequest; here we
        // additionally map the whitelisted name to its qualified form.
        const std::string safeOrderBy   = qualifiedSortColumn(orderBy);
        const std::string safeDirection = (orderDirection == "DESC")
                                              ? "DESC"
                                              : "ASC";

        try {
            // --- Count ---
            auto countRows = co_await db->execSqlCoro(
                "SELECT COUNT(*) AS total "
                "FROM endpoints e "
                "JOIN processes pr        ON pr.id = e.process_id "
                "JOIN service_metadata sm ON sm.id = pr.service_metadata_id "
                "JOIN participants p      ON p.id  = sm.participant_id "
                "WHERE p.tenant_id = $1 "
                "AND ("
                "  $2 = '' OR "
                "  p.identifier_value    ILIKE '%' || $2 || '%' OR "
                "  sm.document_type_value ILIKE '%' || $2 || '%' OR "
                "  pr.process_value       ILIKE '%' || $2 || '%' OR "
                "  e.endpoint_url         ILIKE '%' || $2 || '%'"
                ")",
                tenantId, search
            );
            result.total = countRows[0]["total"].as<int>();

            // --- Data ---
            const std::string dataSql =
                "SELECT "
                "  e.id                         AS endpoint_id, "
                "  e.endpoint_url               AS endpoint_url, "
                "  e.transport_profile          AS transport_profile, "
                "  e.require_bls                AS require_bls, "
                "  e.service_activation_date    AS service_activation_date, "
                "  e.service_expiration_date    AS service_expiration_date, "
                "  e.created_at                 AS endpoint_created_at, "
                "  p.id                         AS participant_id, "
                "  p.identifier_scheme          AS identifier_scheme, "
                "  p.identifier_value           AS identifier_value, "
                "  sm.id                        AS service_metadata_id, "
                "  sm.document_type_scheme      AS document_type_scheme, "
                "  sm.document_type_value       AS document_type_value, "
                "  pr.id                        AS process_id, "
                "  pr.process_scheme            AS process_scheme, "
                "  pr.process_value             AS process_value "
                "FROM endpoints e "
                "JOIN processes pr        ON pr.id = e.process_id "
                "JOIN service_metadata sm ON sm.id = pr.service_metadata_id "
                "JOIN participants p      ON p.id  = sm.participant_id "
                "WHERE p.tenant_id = $1 "
                "AND ("
                "  $2 = '' OR "
                "  p.identifier_value    ILIKE '%' || $2 || '%' OR "
                "  sm.document_type_value ILIKE '%' || $2 || '%' OR "
                "  pr.process_value       ILIKE '%' || $2 || '%' OR "
                "  e.endpoint_url         ILIKE '%' || $2 || '%'"
                ") "
                "ORDER BY " + safeOrderBy + " " + safeDirection + ", e.id ASC "
                "LIMIT $3 OFFSET $4";

            auto rows = co_await db->execSqlCoro(
                dataSql, tenantId, search, limit, offset
            );

            for (const auto& row : rows) {
                result.items.push_back(rowToAudit(row));
            }

        } catch (const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "[EndpointAuditRepository] findPagedByTenantId: "
                      << e.base().what();
        }

        co_return result;
    }

} // namespace SMPPlusPlus
