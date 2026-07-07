//
// Created by Saadat Baig on 07.07.26.
//
#include "public_participant_repository.h"


namespace SMPPlusPlus
{
    drogon::Task<PublicParticipantRepository::PagedPublicParticipants>
    PublicParticipantRepository::findPagedDistinct(
        int                limit,
        int                offset,
        const std::string& orderBy,
        const std::string& orderDirection,
        const std::string& search)
    {
        auto db = drogon::app().getFastDbClient("default");
        PagedPublicParticipants result;

        // Whitelisted upstream by Pagination::fromRequest.
        // Only "identifier_value" and "identifier_scheme" are allowed for this
        // endpoint - the whitelist is enforced at the controller layer.
        const std::string safeOrderBy   = orderBy.empty()
                                              ? "identifier_value"
                                              : orderBy;
        const std::string safeDirection = (orderDirection == "DESC")
                                              ? "DESC"
                                              : "ASC";

        try {
            // Count of distinct (scheme, value) pairs matching the search.
            auto countRows = co_await db->execSqlCoro(
                "SELECT COUNT(*) AS total FROM ("
                "  SELECT DISTINCT identifier_scheme, identifier_value "
                "  FROM participants "
                "  WHERE $1 = '' OR "
                "        identifier_value  ILIKE '%' || $1 || '%' OR "
                "        identifier_scheme ILIKE '%' || $1 || '%'"
                ") AS distinct_participants",
                search
            );
            result.total = countRows[0]["total"].as<int>();

            // Distinct data page. ORDER BY column reference is safe because
            // it comes from the whitelisted set. Secondary sort by the other
            // identifier column ensures stable ordering across pages.
            const std::string secondarySort =
                (safeOrderBy == "identifier_value")
                    ? "identifier_scheme"
                    : "identifier_value";

            const std::string dataSql =
                "SELECT DISTINCT identifier_scheme, identifier_value "
                "FROM participants "
                "WHERE $1 = '' OR "
                "      identifier_value  ILIKE '%' || $1 || '%' OR "
                "      identifier_scheme ILIKE '%' || $1 || '%' "
                "ORDER BY " + safeOrderBy + " " + safeDirection +
                ", " + secondarySort + " ASC "
                "LIMIT $2::integer OFFSET $3::integer";

            auto rows = co_await db->execSqlCoro(
                dataSql, search, limit, offset
            );

            for (const auto& row : rows) {
                PublicParticipantRow r;
                r.identifierScheme = row["identifier_scheme"].as<std::string>();
                r.identifierValue  = row["identifier_value"].as<std::string>();
                result.items.push_back(std::move(r));
            }

        } catch (const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "[PublicParticipantRepository] findPagedDistinct: "
                      << e.base().what();
        }

        co_return result;
    }

} // namespace SMPPlusPlus
