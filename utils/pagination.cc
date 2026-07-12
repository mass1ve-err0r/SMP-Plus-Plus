//
// Created by Saadat Baig on 07.07.26.
//
#include "pagination.h"

#include <charconv>
#include <cctype>


namespace SMPPlusPlus
{
    int
    Pagination::parseIntParam(const drogon::HttpRequestPtr& req, const char* name, int defaultValue, int minValue, int maxValue)
    {
        const std::string val = req->getParameter(name);
        if (val.empty()) return defaultValue;

        int parsed = 0;
        auto [ptr, ec] = std::from_chars(val.data(), val.data() + val.size(), parsed);
        if (ec != std::errc{}) return defaultValue;

        return std::clamp(parsed, minValue, maxValue);
    }

    bool
    Pagination::isAllowedColumn(std::string_view column, const std::initializer_list<std::string_view>& allowed)
    {
        for (auto a : allowed)
        {
            if (a == column) return true;
        }

        return false;
    }

    Pagination
    Pagination::fromRequest(const drogon::HttpRequestPtr& req, const std::initializer_list<std::string_view>& allowedOrderColumns, std::string_view defaultOrderBy)
    {
        Pagination p;

        // limit - clamped to [MIN_LIMIT, MAX_LIMIT]
        p.limit = parseIntParam(req, "limit", DEFAULT_LIMIT, MIN_LIMIT, MAX_LIMIT);

        // offset - clamped to [0, INT_MAX]
        p.offset = parseIntParam(req, "offset", 0, 0, std::numeric_limits<int>::max());

        // orderBy - whitelist check, else default
        const std::string requested = req->getParameter("orderBy");
        p.orderBy = isAllowedColumn(requested, allowedOrderColumns)
                        ? requested
                        : std::string(defaultOrderBy);

        // orderDirection - normalize to upper, only ASC or DESC accepted
        std::string dir = req->getParameter("orderDirection");
        for (auto& c : dir) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        p.orderDirection = (dir == "DESC") ? "DESC" : "ASC";

        // search - passed through as-is. The repository is responsible for
        // using it as a parameter, not concatenating into SQL.
        // InputSanitizer covers general-purpose forbidden characters upstream,
        // and ILIKE with placeholder ($n) parameters is injection-safe.
        p.search = req->getParameter("search");

        return p;
    }

} // namespace SMPPlusPlus
