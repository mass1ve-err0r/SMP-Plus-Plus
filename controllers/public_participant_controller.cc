//
// Created by Saadat Baig on 08.07.26.
//
#include "public_participant_controller.h"
#include "db/public_participant_repository.h"
#include "utils/pagination.h"
#include "utils/url_utils.h"

#include <drogon/drogon.h>


namespace SMPPlusPlus
{
    namespace {

        drogon::HttpResponsePtr jsonOk(const Json::Value& body) {
            auto resp = drogon::HttpResponse::newHttpJsonResponse(body);
            resp->setStatusCode(drogon::k200OK);
            // Public directory - cheap to serve, safe to cache for 60s.
            // Downstream caches (nginx, Cloudflare) and browsers respect this.
            resp->addHeader("Cache-Control", "public, max-age=60");
            return resp;
        }

        Json::Value rowToJson(const PublicParticipantRow& r) {
            Json::Value v;
            v["identifier_scheme"]      = r.identifierScheme;
            v["identifier_value"]       = r.identifierValue;
            // Canonical combined form for display and for constructing SMP URLs.
            v["participant_identifier"] = r.identifierScheme + "::" + r.identifierValue;
            // Percent-encoded form the UI can drop straight into href="/{...}"
            // to hit the SMP protocol GET /{participantId} endpoint.
            v["encoded_href"]           = "/" + UrlUtils::encodePeppolId(
                r.identifierScheme, r.identifierValue);
            return v;
        }

    } // anonymous namespace

    // -----------------------------------------------------------------------------
    // GET /participants
    // Query params: ?limit&offset&orderBy&orderDirection&search
    // -----------------------------------------------------------------------------
    drogon::Task<> PublicParticipantController::listParticipants(
        drogon::HttpRequestPtr req,
        std::function<void(const drogon::HttpResponsePtr&)> callback)
    {
        const auto p = Pagination::fromRequest(
            req,
            {"identifier_value", "identifier_scheme"},
            "identifier_value"
        );

        auto page = co_await PublicParticipantRepository::findPagedDistinct(
            p.limit,
            p.offset,
            p.orderBy,
            p.orderDirection,
            p.search
        );

        Json::Value items(Json::arrayValue);
        for (const auto& row : page.items)
            items.append(rowToJson(row));

        Json::Value body;
        body["items"]  = items;
        body["total"]  = page.total;
        body["limit"]  = p.limit;
        body["offset"] = p.offset;

        callback(jsonOk(body));
    }

} // namespace SMPPlusPlus
