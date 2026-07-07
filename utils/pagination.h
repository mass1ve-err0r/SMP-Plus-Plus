//
// Created by Saadat Baig on 07.07.26.
//
#pragma once

#include <string>
#include <string_view>
#include <initializer_list>
#include <algorithm>
#include <drogon/HttpRequest.h>


namespace SMPPlusPlus
{
    /**
     * Pagination - Shared query-parameter parsing for paginated list endpoints.
     *
     * Stateless, thread-safe. All methods are static or operate on stack-local
     * data. No shared mutable state.
     *
     * Query parameters:
     *   ?limit=25            (default 25, min 1, max 200)
     *   ?offset=0            (default 0, min 0)
     *   ?orderBy=name        (whitelisted per endpoint - see fromRequest)
     *   ?orderDirection=ASC  (ASC or DESC, default ASC)
     *   ?search=foo          (optional ILIKE substring match, default empty)
     *
     * Typical controller usage:
     *
     *   const auto p = Pagination::fromRequest(
     *       req,
     *       {"name", "slug", "created_at"},  // allowed sort columns
     *       "name"                             // default sort column
     *   );
     *
     *   auto page = co_await Repo::findPaged(p);
     *
     *   Json::Value body;
     *   body["items"]  = ...;
     *   body["total"]  = page.total;
     *   body["limit"]  = p.limit;
     *   body["offset"] = p.offset;
     *
     * Response envelope shape (returned by controllers):
     *   {
     *     "items":  [...],
     *     "total":  147,
     *     "limit":  25,
     *     "offset": 0
     *   }
     */
    struct Pagination {
        int         limit          = 25;
        int         offset         = 0;
        std::string orderBy        = "";     // empty = repository default
        std::string orderDirection = "ASC";  // always "ASC" or "DESC"
        std::string search         = "";     // may be empty

        // Bounds enforced during parsing
        static constexpr int MIN_LIMIT     = 1;
        static constexpr int MAX_LIMIT     = 200;
        static constexpr int DEFAULT_LIMIT = 25;

        /**
         * Parse pagination parameters from a request's query string.
         *
         * @param req                   The HTTP request
         * @param allowedOrderColumns   Whitelist of column names that may be used
         *                              in ORDER BY. Anything else falls back to
         *                              defaultOrderBy. This IS the SQL-safety guard
         *                              for ORDER BY - we cannot use $1 placeholders
         *                              for column names, so we whitelist instead.
         * @param defaultOrderBy        Column used if no orderBy is provided, or
         *                              if the provided orderBy is not in the whitelist.
         */
        static Pagination fromRequest(
            const drogon::HttpRequestPtr& req,
            const std::initializer_list<std::string_view>& allowedOrderColumns,
            std::string_view defaultOrderBy);

    private:
        static int  parseIntParam(
            const drogon::HttpRequestPtr& req,
            const char* name,
            int defaultValue,
            int minValue,
            int maxValue);

        static bool isAllowedColumn(
            std::string_view column,
            const std::initializer_list<std::string_view>& allowed);
    };

} // namespace SMPPlusPlus
