//
// Created by Saadat Baig on 29.06.26.
//
#include "sml_controller.h"
#include "db/sml_repository.h"

#include <drogon/drogon.h>


namespace SMPPlusPlus
{
    namespace {

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

        Json::Value smlToJson(const SmlConfiguration& s) {
            Json::Value v;
            v["id"]                  = s.id;
            v["label"]               = s.label;
            v["dns_zone"]            = s.dnsZone;
            v["management_endpoint"] = s.managementEndpoint;
            v["smp_id"]              = s.smpId;
            v["is_primary"]          = s.isPrimary;
            v["is_enabled"]          = s.isEnabled;
            v["auto_register"]       = s.autoRegister;
            v["created_at"]          = s.createdAt;
            return v;
        }

    } // anonymous namespace

    // GET /api/management/sml
    drogon::Task<> SmlController::getAll(
        drogon::HttpRequestPtr req,
        std::function<void(const drogon::HttpResponsePtr&)> callback)
    {
        auto smls = co_await SmlRepository::findAll();

        Json::Value body(Json::arrayValue);
        for (const auto& s : smls)
            body.append(smlToJson(s));

        callback(jsonOk(body));
    }

    // GET /api/management/sml/:id
    drogon::Task<> SmlController::getOne(
        drogon::HttpRequestPtr req,
        std::function<void(const drogon::HttpResponsePtr&)> callback,
        std::string id)
    {
        auto sml = co_await SmlRepository::findById(id);
        if (!sml) {
            callback(jsonError(drogon::k404NotFound, "SML configuration not found"));
            co_return;
        }
        callback(jsonOk(smlToJson(*sml)));
    }

    // POST /api/management/sml
    // Body: {
    //   "label":                "Peppol Production",
    //   "dns_zone":             "edelivery.tech.ec.europa.eu",
    //   "management_endpoint":  "https://...",
    //   "smp_id":               "SMP-001",
    //   "is_primary":           true,   // optional, default false
    //   "is_enabled":           true,   // optional, default false
    //   "auto_register":        false   // optional, default false
    // }
    drogon::Task<> SmlController::create(
        drogon::HttpRequestPtr req,
        std::function<void(const drogon::HttpResponsePtr&)> callback)
    {
        const auto body = req->getJsonObject();
        if (!body) {
            callback(jsonError(drogon::k400BadRequest, "JSON body required"));
            co_return;
        }

        const auto require = [&](const char* field) -> std::string {
            if (!body->isMember(field) || (*body)[field].asString().empty())
                return "";
            return (*body)[field].asString();
        };

        const std::string label      = require("label");
        const std::string dnsZone    = require("dns_zone");
        const std::string endpoint   = require("management_endpoint");
        const std::string smpId      = require("smp_id");

        if (label.empty()) {
            callback(jsonError(drogon::k400BadRequest, "Field 'label' is required"));
            co_return;
        }
        if (dnsZone.empty()) {
            callback(jsonError(drogon::k400BadRequest, "Field 'dns_zone' is required"));
            co_return;
        }
        if (endpoint.empty()) {
            callback(jsonError(drogon::k400BadRequest,
                "Field 'management_endpoint' is required"));
            co_return;
        }
        if (smpId.empty()) {
            callback(jsonError(drogon::k400BadRequest, "Field 'smp_id' is required"));
            co_return;
        }

        const bool isPrimary     = body->isMember("is_primary")
                                    ? (*body)["is_primary"].asBool() : false;
        const bool isEnabled     = body->isMember("is_enabled")
                                    ? (*body)["is_enabled"].asBool() : false;
        const bool autoRegister  = body->isMember("auto_register")
                                    ? (*body)["auto_register"].asBool() : false;

        const std::string newId = co_await SmlRepository::create(
            label, dnsZone, endpoint, smpId,
            isPrimary, isEnabled, autoRegister
        );

        if (newId.empty()) {
            callback(jsonError(drogon::k500InternalServerError,
                "Failed to create SML configuration"));
            co_return;
        }

        auto created = co_await SmlRepository::findById(newId);
        callback(jsonCreated(smlToJson(*created)));
    }

    // PUT /api/management/sml/:id
    // Body: same fields as POST except is_primary/is_enabled
    // (use dedicated /primary, /enable, /disable endpoints for those)
    drogon::Task<> SmlController::update(
        drogon::HttpRequestPtr req,
        std::function<void(const drogon::HttpResponsePtr&)> callback,
        std::string id)
    {
        auto existing = co_await SmlRepository::findById(id);
        if (!existing) {
            callback(jsonError(drogon::k404NotFound, "SML configuration not found"));
            co_return;
        }

        const auto body = req->getJsonObject();
        if (!body) {
            callback(jsonError(drogon::k400BadRequest, "JSON body required"));
            co_return;
        }

        // Merge with existing values — only update provided fields
        const std::string label    = body->isMember("label") &&
                                     !(*body)["label"].asString().empty()
                                        ? (*body)["label"].asString()
                                        : existing->label;
        const std::string dnsZone  = body->isMember("dns_zone") &&
                                     !(*body)["dns_zone"].asString().empty()
                                        ? (*body)["dns_zone"].asString()
                                        : existing->dnsZone;
        const std::string endpoint = body->isMember("management_endpoint") &&
                                     !(*body)["management_endpoint"].asString().empty()
                                        ? (*body)["management_endpoint"].asString()
                                        : existing->managementEndpoint;
        const std::string smpId    = body->isMember("smp_id") &&
                                     !(*body)["smp_id"].asString().empty()
                                        ? (*body)["smp_id"].asString()
                                        : existing->smpId;
        const bool autoRegister    = body->isMember("auto_register")
                                        ? (*body)["auto_register"].asBool()
                                        : existing->autoRegister;

        if (!co_await SmlRepository::update(
                id, label, dnsZone, endpoint, smpId, autoRegister)) {
            callback(jsonError(drogon::k500InternalServerError,
                "Failed to update SML configuration"));
            co_return;
                }

        auto updated = co_await SmlRepository::findById(id);
        callback(jsonOk(smlToJson(*updated)));
    }

    // DELETE /api/management/sml/:id
    drogon::Task<> SmlController::remove(
        drogon::HttpRequestPtr req,
        std::function<void(const drogon::HttpResponsePtr&)> callback,
        std::string id)
    {
        auto existing = co_await SmlRepository::findById(id);
        if (!existing) {
            callback(jsonError(drogon::k404NotFound, "SML configuration not found"));
            co_return;
        }

        if (existing->isPrimary) {
            callback(jsonError(drogon::k400BadRequest,
                "Cannot delete the primary SML configuration. "
                "Set another SML as primary first."));
            co_return;
        }

        if (!co_await SmlRepository::remove(id)) {
            callback(jsonError(drogon::k500InternalServerError,
                "Failed to delete SML configuration"));
            co_return;
        }

        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::k204NoContent);
        callback(resp);
    }

    // PATCH /api/management/sml/:id/enable
    drogon::Task<> SmlController::enable(
        drogon::HttpRequestPtr req,
        std::function<void(const drogon::HttpResponsePtr&)> callback,
        std::string id)
    {
        auto existing = co_await SmlRepository::findById(id);
        if (!existing) {
            callback(jsonError(drogon::k404NotFound, "SML configuration not found"));
            co_return;
        }

        if (existing->isEnabled) {
            callback(jsonError(drogon::k409Conflict,
                "SML configuration is already enabled"));
            co_return;
        }

        if (!co_await SmlRepository::enable(id)) {
            callback(jsonError(drogon::k500InternalServerError,
                "Failed to enable SML configuration"));
            co_return;
        }

        auto updated = co_await SmlRepository::findById(id);
        callback(jsonOk(smlToJson(*updated)));
    }

    // PATCH /api/management/sml/:id/disable
    drogon::Task<> SmlController::disable(
        drogon::HttpRequestPtr req,
        std::function<void(const drogon::HttpResponsePtr&)> callback,
        std::string id)
    {
        auto existing = co_await SmlRepository::findById(id);
        if (!existing) {
            callback(jsonError(drogon::k404NotFound, "SML configuration not found"));
            co_return;
        }

        if (!existing->isEnabled) {
            callback(jsonError(drogon::k409Conflict,
                "SML configuration is already disabled"));
            co_return;
        }

        if (!co_await SmlRepository::disable(id)) {
            callback(jsonError(drogon::k500InternalServerError,
                "Failed to disable SML configuration"));
            co_return;
        }

        auto updated = co_await SmlRepository::findById(id);
        callback(jsonOk(smlToJson(*updated)));
    }

    // PATCH /api/management/sml/:id/primary
    drogon::Task<> SmlController::setPrimary(
        drogon::HttpRequestPtr req,
        std::function<void(const drogon::HttpResponsePtr&)> callback,
        std::string id)
    {
        auto existing = co_await SmlRepository::findById(id);
        if (!existing) {
            callback(jsonError(drogon::k404NotFound, "SML configuration not found"));
            co_return;
        }

        if (existing->isPrimary) {
            callback(jsonError(drogon::k409Conflict,
                "SML configuration is already primary"));
            co_return;
        }

        if (!co_await SmlRepository::setPrimary(id)) {
            callback(jsonError(drogon::k500InternalServerError,
                "Failed to set primary SML configuration"));
            co_return;
        }

        auto updated = co_await SmlRepository::findById(id);
        callback(jsonOk(smlToJson(*updated)));
    }

} // namespace SMPPlusPlus
