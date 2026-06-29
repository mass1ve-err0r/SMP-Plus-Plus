//
// Created by Saadat Baig on 28.06.26.
//
#include "tenant_controller.h"
#include "db/tenant_repository.h"

#include <drogon/drogon.h>


namespace SMPPlusPlus
{
    namespace {

        // -----------------------------------------------------------------------------
        // Helpers
        // -----------------------------------------------------------------------------
        drogon::HttpResponsePtr jsonOk(const Json::Value& body) {
            auto resp = drogon::HttpResponse::newHttpJsonResponse(body);
            resp->setStatusCode(drogon::k200OK);
            return resp;
        }

        drogon::HttpResponsePtr jsonCreated(const Json::Value& body) {
            auto resp = drogon::HttpResponse::newHttpJsonResponse(body);
            resp->setStatusCode(drogon::k201Created);
            return resp;
        }

        drogon::HttpResponsePtr jsonError(drogon::HttpStatusCode code,
                                          const std::string& message) {
            Json::Value body;
            body["error"] = message;
            auto resp = drogon::HttpResponse::newHttpJsonResponse(body);
            resp->setStatusCode(code);
            return resp;
        }

        bool isCoreAdmin(const drogon::HttpRequestPtr& req) {
            return req->getAttributes()->get<std::string>("auth_user_role") == "CORE_ADMIN";
        }

        std::string authUserId(const drogon::HttpRequestPtr& req) {
            return req->getAttributes()->get<std::string>("auth_user_id");
        }

        Json::Value tenantToJson(const Tenant& t) {
            Json::Value v;
            v["id"]         = t.id;
            v["name"]       = t.name;
            v["slug"]       = t.slug;
            v["created_at"] = t.createdAt;
            v["created_by"] = t.createdBy;
            return v;
        }

        // Slugify: lowercase, spaces to hyphens, strip non-alphanumeric except hyphens
        std::string slugify(const std::string& name) {
            std::string slug;
            slug.reserve(name.size());
            for (unsigned char c : name) {
                if (std::isalnum(c))
                    slug += static_cast<char>(std::tolower(c));
                else if (std::isspace(c) || c == '-' || c == '_')
                    slug += '-';
            }
            // Collapse consecutive hyphens
            std::string clean;
            bool lastWasHyphen = false;
            for (char c : slug) {
                if (c == '-') {
                    if (!lastWasHyphen) clean += c;
                    lastWasHyphen = true;
                } else {
                    clean += c;
                    lastWasHyphen = false;
                }
            }
            // Trim leading/trailing hyphens
            const auto start = clean.find_first_not_of('-');
            const auto end   = clean.find_last_not_of('-');
            if (start == std::string::npos) return "";
            return clean.substr(start, end - start + 1);
        }

    } // anonymous namespace

    // -----------------------------------------------------------------------------
    // GET /api/management/tenants
    // CORE_ADMIN: all tenants
    // MANAGEMENT: only granted tenants
    // -----------------------------------------------------------------------------
    drogon::Task<> TenantController::getAll(
        drogon::HttpRequestPtr req,
        std::function<void(const drogon::HttpResponsePtr&)> callback)
    {
        std::vector<Tenant> tenants;

        if (isCoreAdmin(req)) {
            tenants = co_await TenantRepository::findAll();
        } else {
            tenants = co_await TenantRepository::findByUserId(authUserId(req));
        }

        Json::Value body(Json::arrayValue);
        for (const auto& t : tenants)
            body.append(tenantToJson(t));

        callback(jsonOk(body));
    }

    // -----------------------------------------------------------------------------
    // GET /api/management/tenants/:id
    // -----------------------------------------------------------------------------
    drogon::Task<> TenantController::getOne(
        drogon::HttpRequestPtr req,
        std::function<void(const drogon::HttpResponsePtr&)> callback,
        std::string id)
    {
        auto tenant = co_await TenantRepository::findById(id);
        if (!tenant) {
            callback(jsonError(drogon::k404NotFound, "Tenant not found"));
            co_return;
        }

        // MANAGEMENT users can only see tenants they have access to
        if (!isCoreAdmin(req)) {
            auto granted = co_await TenantRepository::findByUserId(authUserId(req));
            bool hasAccess = false;
            for (const auto& t : granted) {
                if (t.id == id) { hasAccess = true; break; }
            }
            if (!hasAccess) {
                callback(jsonError(drogon::k403Forbidden, "Access denied"));
                co_return;
            }
        }

        callback(jsonOk(tenantToJson(*tenant)));
    }

    // -----------------------------------------------------------------------------
    // POST /api/management/tenants
    // CORE_ADMIN only
    // Body: { "name": "...", "slug": "..." (optional, auto-generated if missing) }
    // -----------------------------------------------------------------------------
    drogon::Task<> TenantController::create(
        drogon::HttpRequestPtr req,
        std::function<void(const drogon::HttpResponsePtr&)> callback)
    {
        if (!isCoreAdmin(req)) {
            callback(jsonError(drogon::k403Forbidden, "Only CORE_ADMIN can create tenants"));
            co_return;
        }

        const auto body = req->getJsonObject();
        if (!body || !body->isMember("name") || (*body)["name"].asString().empty()) {
            callback(jsonError(drogon::k400BadRequest, "Field 'name' is required"));
            co_return;
        }

        const std::string name = (*body)["name"].asString();
        std::string slug = body->isMember("slug") && !(*body)["slug"].asString().empty()
            ? (*body)["slug"].asString()
            : slugify(name);

        if (slug.empty()) {
            callback(jsonError(drogon::k400BadRequest, "Could not generate a valid slug from name"));
            co_return;
        }

        // Slug uniqueness check
        if (co_await TenantRepository::slugExists(slug)) {
            callback(jsonError(drogon::k409Conflict,
                "Slug '" + slug + "' is already taken"));
            co_return;
        }

        const std::string newId = co_await TenantRepository::create(
            name, slug, authUserId(req)
        );

        if (newId.empty()) {
            callback(jsonError(drogon::k500InternalServerError, "Failed to create tenant"));
            co_return;
        }

        auto created = co_await TenantRepository::findById(newId);
        if (!created) {
            callback(jsonError(drogon::k500InternalServerError, "Tenant created but not found"));
            co_return;
        }

        callback(jsonCreated(tenantToJson(*created)));
    }

    // -----------------------------------------------------------------------------
    // PUT /api/management/tenants/:id
    // CORE_ADMIN only
    // Body: { "name": "...", "slug": "..." }
    // -----------------------------------------------------------------------------
    drogon::Task<> TenantController::update(
        drogon::HttpRequestPtr req,
        std::function<void(const drogon::HttpResponsePtr&)> callback,
        std::string id)
    {
        if (!isCoreAdmin(req)) {
            callback(jsonError(drogon::k403Forbidden, "Only CORE_ADMIN can update tenants"));
            co_return;
        }

        auto existing = co_await TenantRepository::findById(id);
        if (!existing) {
            callback(jsonError(drogon::k404NotFound, "Tenant not found"));
            co_return;
        }

        const auto body = req->getJsonObject();
        if (!body) {
            callback(jsonError(drogon::k400BadRequest, "JSON body required"));
            co_return;
        }

        const std::string name = body->isMember("name") && !(*body)["name"].asString().empty()
            ? (*body)["name"].asString()
            : existing->name;

        const std::string slug = body->isMember("slug") && !(*body)["slug"].asString().empty()
            ? (*body)["slug"].asString()
            : existing->slug;

        // Check slug uniqueness only if it changed
        if (slug != existing->slug && co_await TenantRepository::slugExists(slug)) {
            callback(jsonError(drogon::k409Conflict,
                "Slug '" + slug + "' is already taken"));
            co_return;
        }

        const bool updated = co_await TenantRepository::update(id, name, slug);
        if (!updated) {
            callback(jsonError(drogon::k500InternalServerError, "Failed to update tenant"));
            co_return;
        }

        auto updated_tenant = co_await TenantRepository::findById(id);
        callback(jsonOk(tenantToJson(*updated_tenant)));
    }

    // -----------------------------------------------------------------------------
    // DELETE /api/management/tenants/:id
    // CORE_ADMIN only
    // Cascades to participants, service_metadata, processes, endpoints
    // via ON DELETE CASCADE in the schema.
    // -----------------------------------------------------------------------------
    drogon::Task<> TenantController::remove(
        drogon::HttpRequestPtr req,
        std::function<void(const drogon::HttpResponsePtr&)> callback,
        std::string id)
    {
        if (!isCoreAdmin(req)) {
            callback(jsonError(drogon::k403Forbidden, "Only CORE_ADMIN can delete tenants"));
            co_return;
        }

        auto existing = co_await TenantRepository::findById(id);
        if (!existing) {
            callback(jsonError(drogon::k404NotFound, "Tenant not found"));
            co_return;
        }

        const bool removed = co_await TenantRepository::remove(id);
        if (!removed) {
            callback(jsonError(drogon::k500InternalServerError, "Failed to delete tenant"));
            co_return;
        }

        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::k204NoContent);
        callback(resp);
    }

} // namespace SMPPlusPlus
