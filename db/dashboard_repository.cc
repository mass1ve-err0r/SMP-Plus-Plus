//
// Created by Saadat Baig on 08.07.26.
//
#include "dashboard_repository.h"


namespace SMPPlusPlus
{
    drogon::Task<DashboardCounts> DashboardRepository::getCounts(
        const std::string& userId,
        bool               isCoreAdmin)
    {
        auto db = drogon::app().getFastDbClient("default");
        DashboardCounts result;

        try {
            // Single round trip. Two variants because the WHERE clauses differ
            // by scope - CORE_ADMIN sees everything, MANAGEMENT sees only their
            // granted tenants. Both use CTEs so all four counts come back in
            // one query.
            drogon::orm::Result rows(nullptr);

            if (isCoreAdmin) {
                rows = co_await db->execSqlCoro(
                    "WITH "
                    "t  AS (SELECT COUNT(*) AS c FROM tenants), "
                    "p  AS (SELECT COUNT(*) AS c FROM participants), "
                    "sm AS (SELECT COUNT(*) AS c FROM service_metadata), "
                    "e  AS (SELECT COUNT(*) AS c FROM endpoints) "
                    "SELECT t.c AS tenants, p.c AS participants, "
                    "       sm.c AS services, e.c AS endpoints "
                    "FROM t, p, sm, e"
                );
            } else {
                // MANAGEMENT: scope everything through user_tenant_grants.
                rows = co_await db->execSqlCoro(
                    "WITH "
                    "granted AS ("
                    "  SELECT tenant_id FROM user_tenant_grants WHERE user_id = $1"
                    "), "
                    "t  AS (SELECT COUNT(*) AS c FROM granted), "
                    "p  AS ("
                    "  SELECT COUNT(*) AS c FROM participants "
                    "  WHERE tenant_id IN (SELECT tenant_id FROM granted)"
                    "), "
                    "sm AS ("
                    "  SELECT COUNT(*) AS c FROM service_metadata sm2 "
                    "  JOIN participants p2 ON p2.id = sm2.participant_id "
                    "  WHERE p2.tenant_id IN (SELECT tenant_id FROM granted)"
                    "), "
                    "e AS ("
                    "  SELECT COUNT(*) AS c FROM endpoints e2 "
                    "  JOIN processes pr2        ON pr2.id  = e2.process_id "
                    "  JOIN service_metadata sm3 ON sm3.id  = pr2.service_metadata_id "
                    "  JOIN participants p3      ON p3.id   = sm3.participant_id "
                    "  WHERE p3.tenant_id IN (SELECT tenant_id FROM granted)"
                    ") "
                    "SELECT t.c AS tenants, p.c AS participants, "
                    "       sm.c AS services, e.c AS endpoints "
                    "FROM t, p, sm, e",
                    userId
                );
            }

            if (!rows.empty()) {
                const auto& row = rows[0];
                result.tenants      = row["tenants"].as<int>();
                result.participants = row["participants"].as<int>();
                result.services     = row["services"].as<int>();
                result.endpoints    = row["endpoints"].as<int>();
            }

        } catch (const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "[DashboardRepository] getCounts: " << e.base().what();
        }

        co_return result;
    }

} // namespace SMPPlusPlus
