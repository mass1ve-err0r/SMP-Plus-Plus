//
// Created by Saadat Baig on 29.06.26.
//
#include "user_controller.h"
#include "db/user_repository.h"
#include "db/tenant_repository.h"
#include "utils/crypto_utils.h"
#include "utils/environment.h"

#include <drogon/drogon.h>
#include <random>
#include <sstream>
#include <iomanip>


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

        // Strip password_hash from user JSON - never expose it
        Json::Value userToJson(const User& u, bool includeGrantsHint = false) {
            Json::Value v;
            v["id"]         = u.id;
            v["username"]   = u.username;
            v["role"]       = u.role;
            v["is_active"]  = u.isActive;
            v["created_at"] = u.createdAt;
            v["created_by"] = u.createdBy;
            return v;
        }

        Json::Value grantToJson(const TenantGrant& g) {
            Json::Value v;
            v["tenant_id"]   = g.tenantId;
            v["tenant_name"] = g.tenantName;
            v["tenant_slug"] = g.tenantSlug;
            v["granted_by"]  = g.grantedBy;
            v["granted_at"]  = g.grantedAt;
            return v;
        }

        // Generate a secure random password: 16 chars, alphanumeric + symbols
        std::string generateRandomPassword() {
            auto randomBytes = CryptoUtils::randomBytes(24);
            if (!randomBytes) return "ChangeMe123!"; // fallback, should never happen
            return CryptoUtils::base64Encode(
                std::span<const uint8_t>(randomBytes->data(), randomBytes->size())
            ).substr(0, 16);
        }

        // bcrypt hash offloaded to DB callback thread via execSqlAsync pattern
        // For controller use we call it directly since we're in a coroutine
        // that drogon::async_run can handle
        drogon::Task<std::string> hashPassword(const std::string& password) {
            auto result = CryptoUtils::bcryptHash(password);
            if (!result) co_return std::string{};
            co_return *result;
        }

    } // anonymous namespace

    // =============================================================================
    // Users
    // =============================================================================

    // GET /api/management/users
    drogon::Task<> UserController::getAll(
        drogon::HttpRequestPtr req,
        std::function<void(const drogon::HttpResponsePtr&)> callback)
    {
        if (!isCoreAdmin(req)) {
            callback(jsonError(drogon::k403Forbidden,
                "Only CORE_ADMIN can list users"));
            co_return;
        }

        auto users = co_await UserRepository::findAll();

        Json::Value body(Json::arrayValue);
        for (const auto& u : users)
            body.append(userToJson(u));

        callback(jsonOk(body));
    }

    // GET /api/management/users/:id
    drogon::Task<> UserController::getOne(
        drogon::HttpRequestPtr req,
        std::function<void(const drogon::HttpResponsePtr&)> callback,
        std::string id)
    {
        if (!isCoreAdmin(req)) {
            callback(jsonError(drogon::k403Forbidden,
                "Only CORE_ADMIN can view users"));
            co_return;
        }

        auto user = co_await UserRepository::findById(id);
        if (!user) {
            callback(jsonError(drogon::k404NotFound, "User not found"));
            co_return;
        }

        callback(jsonOk(userToJson(*user)));
    }

    // POST /api/management/users
    // Body: {
    //   "username": "john",
    //   "password": "..."   // optional - generated if omitted
    // }
    drogon::Task<> UserController::create(
        drogon::HttpRequestPtr req,
        std::function<void(const drogon::HttpResponsePtr&)> callback)
    {
        if (!isCoreAdmin(req)) {
            callback(jsonError(drogon::k403Forbidden,
                "Only CORE_ADMIN can create users"));
            co_return;
        }

        const auto body = req->getJsonObject();
        if (!body || !body->isMember("username") ||
            (*body)["username"].asString().empty())
        {
            callback(jsonError(drogon::k400BadRequest,
                "Field 'username' is required"));
            co_return;
        }

        const std::string username = (*body)["username"].asString();

        if (co_await UserRepository::usernameExists(username)) {
            callback(jsonError(drogon::k409Conflict,
                "Username '" + username + "' is already taken"));
            co_return;
        }

        // Determine password
        bool passwordGenerated = false;
        std::string plainPassword;

        if (body->isMember("password") && !(*body)["password"].asString().empty()) {
            plainPassword = (*body)["password"].asString();
        } else {
            plainPassword = generateRandomPassword();
            passwordGenerated = true;
        }

        if (plainPassword.size() > 72) {
            callback(jsonError(drogon::k400BadRequest,
                "Password exceeds maximum length of 72 characters"));
            co_return;
        }

        // bcrypt - blocking but we're in async context via Drogon coroutine
        const std::string hash = co_await hashPassword(plainPassword);
        if (hash.empty()) {
            callback(jsonError(drogon::k500InternalServerError,
                "Failed to hash password"));
            co_return;
        }

        const std::string newId = co_await UserRepository::create(
            username, hash, "MANAGEMENT", authUserId(req)
        );

        if (newId.empty()) {
            callback(jsonError(drogon::k500InternalServerError,
                "Failed to create user"));
            co_return;
        }

        auto created = co_await UserRepository::findById(newId);
        if (!created) {
            callback(jsonError(drogon::k500InternalServerError,
                "User created but not found"));
            co_return;
        }

        Json::Value response = userToJson(*created);

        // Return generated password once - never stored in plaintext,
        // never retrievable again after this response
        if (passwordGenerated) {
            response["generated_password"] = plainPassword;
            response["generated_password_notice"] =
                "Store this password now. It will not be shown again.";
        }

        callback(jsonCreated(response));
    }

    // PATCH /api/management/users/:id/deactivate
    drogon::Task<> UserController::deactivate(
        drogon::HttpRequestPtr req,
        std::function<void(const drogon::HttpResponsePtr&)> callback,
        std::string id)
    {
        if (!isCoreAdmin(req)) {
            callback(jsonError(drogon::k403Forbidden,
                "Only CORE_ADMIN can deactivate users"));
            co_return;
        }

        auto user = co_await UserRepository::findById(id);
        if (!user) {
            callback(jsonError(drogon::k404NotFound, "User not found"));
            co_return;
        }

        if (user->role == "CORE_ADMIN") {
            callback(jsonError(drogon::k400BadRequest,
                "Cannot deactivate the CORE_ADMIN user"));
            co_return;
        }

        if (!user->isActive) {
            callback(jsonError(drogon::k409Conflict,
                "User is already inactive"));
            co_return;
        }

        if (!co_await UserRepository::deactivate(id)) {
            callback(jsonError(drogon::k500InternalServerError,
                "Failed to deactivate user"));
            co_return;
        }

        auto updated = co_await UserRepository::findById(id);
        callback(jsonOk(userToJson(*updated)));
    }

    // PATCH /api/management/users/:id/reactivate
    drogon::Task<> UserController::reactivate(
        drogon::HttpRequestPtr req,
        std::function<void(const drogon::HttpResponsePtr&)> callback,
        std::string id)
    {
        if (!isCoreAdmin(req)) {
            callback(jsonError(drogon::k403Forbidden,
                "Only CORE_ADMIN can reactivate users"));
            co_return;
        }

        auto user = co_await UserRepository::findById(id);
        if (!user) {
            callback(jsonError(drogon::k404NotFound, "User not found"));
            co_return;
        }

        if (user->isActive) {
            callback(jsonError(drogon::k409Conflict,
                "User is already active"));
            co_return;
        }

        if (!co_await UserRepository::reactivate(id)) {
            callback(jsonError(drogon::k500InternalServerError,
                "Failed to reactivate user"));
            co_return;
        }

        auto updated = co_await UserRepository::findById(id);
        callback(jsonOk(userToJson(*updated)));
    }

    // DELETE /api/management/users/:id
    drogon::Task<> UserController::hardDelete(
        drogon::HttpRequestPtr req,
        std::function<void(const drogon::HttpResponsePtr&)> callback,
        std::string id)
    {
        if (!isCoreAdmin(req)) {
            callback(jsonError(drogon::k403Forbidden,
                "Only CORE_ADMIN can permanently delete users"));
            co_return;
        }

        auto user = co_await UserRepository::findById(id);
        if (!user) {
            callback(jsonError(drogon::k404NotFound, "User not found"));
            co_return;
        }

        if (user->role == "CORE_ADMIN") {
            callback(jsonError(drogon::k400BadRequest,
                "Cannot delete the CORE_ADMIN user"));
            co_return;
        }

        if (!co_await UserRepository::hardDelete(id)) {
            callback(jsonError(drogon::k500InternalServerError,
                "Failed to delete user"));
            co_return;
        }

        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::k204NoContent);
        callback(resp);
    }

    // PATCH /api/management/users/:id/password
    // Body: {
    //   "current_password": "...",  // required for MANAGEMENT changing own password
    //   "new_password":     "..."   // required
    // }
    drogon::Task<> UserController::changePassword(
        drogon::HttpRequestPtr req,
        std::function<void(const drogon::HttpResponsePtr&)> callback,
        std::string id)
    {
        const bool isAdmin   = isCoreAdmin(req);
        const bool isSelf    = authUserId(req) == id;

        // MANAGEMENT users can only change their own password
        if (!isAdmin && !isSelf) {
            callback(jsonError(drogon::k403Forbidden,
                "You can only change your own password"));
            co_return;
        }

        auto user = co_await UserRepository::findById(id);
        if (!user) {
            callback(jsonError(drogon::k404NotFound, "User not found"));
            co_return;
        }

        const auto body = req->getJsonObject();
        if (!body || !body->isMember("new_password") ||
            (*body)["new_password"].asString().empty())
        {
            callback(jsonError(drogon::k400BadRequest,
                "Field 'new_password' is required"));
            co_return;
        }

        const std::string newPassword = (*body)["new_password"].asString();

        if (newPassword.size() > 72) {
            callback(jsonError(drogon::k400BadRequest,
                "Password exceeds maximum length of 72 characters"));
            co_return;
        }

        // MANAGEMENT users must provide current password for verification
        if (!isAdmin) {
            if (!body->isMember("current_password") ||
                (*body)["current_password"].asString().empty())
            {
                callback(jsonError(drogon::k400BadRequest,
                    "Field 'current_password' is required"));
                co_return;
            }

            const std::string currentPassword =
                (*body)["current_password"].asString();

            auto verifyResult = CryptoUtils::bcryptVerify(
                currentPassword, user->passwordHash);

            if (!verifyResult || !(*verifyResult)) {
                callback(jsonError(drogon::k401Unauthorized,
                    "Current password is incorrect"));
                co_return;
            }
        }

        const std::string newHash = co_await hashPassword(newPassword);
        if (newHash.empty()) {
            callback(jsonError(drogon::k500InternalServerError,
                "Failed to hash password"));
            co_return;
        }

        if (!co_await UserRepository::updatePassword(id, newHash)) {
            callback(jsonError(drogon::k500InternalServerError,
                "Failed to update password"));
            co_return;
        }

        Json::Value response;
        response["message"] = "Password updated successfully";
        callback(jsonOk(response));
    }

    // =============================================================================
    // Tenant grants
    // =============================================================================

    // GET /api/management/users/:id/grants
    drogon::Task<> UserController::getGrants(
        drogon::HttpRequestPtr req,
        std::function<void(const drogon::HttpResponsePtr&)> callback,
        std::string id)
    {
        if (!isCoreAdmin(req)) {
            callback(jsonError(drogon::k403Forbidden,
                "Only CORE_ADMIN can view user grants"));
            co_return;
        }

        auto user = co_await UserRepository::findById(id);
        if (!user) {
            callback(jsonError(drogon::k404NotFound, "User not found"));
            co_return;
        }

        auto grants = co_await UserRepository::findGrantsByUserId(id);

        Json::Value body(Json::arrayValue);
        for (const auto& g : grants)
            body.append(grantToJson(g));

        callback(jsonOk(body));
    }

    // POST /api/management/users/:id/grants
    // Body: { "tenant_id": "..." }
    drogon::Task<> UserController::addGrant(
        drogon::HttpRequestPtr req,
        std::function<void(const drogon::HttpResponsePtr&)> callback,
        std::string id)
    {
        if (!isCoreAdmin(req)) {
            callback(jsonError(drogon::k403Forbidden,
                "Only CORE_ADMIN can assign tenant access"));
            co_return;
        }

        auto user = co_await UserRepository::findById(id);
        if (!user) {
            callback(jsonError(drogon::k404NotFound, "User not found"));
            co_return;
        }

        if (user->role == "CORE_ADMIN") {
            callback(jsonError(drogon::k400BadRequest,
                "CORE_ADMIN has access to all tenants — grants are not applicable"));
            co_return;
        }

        const auto body = req->getJsonObject();
        if (!body || !body->isMember("tenant_id") ||
            (*body)["tenant_id"].asString().empty())
        {
            callback(jsonError(drogon::k400BadRequest,
                "Field 'tenant_id' is required"));
            co_return;
        }

        const std::string tenantId = (*body)["tenant_id"].asString();

        auto tenant = co_await TenantRepository::findById(tenantId);
        if (!tenant) {
            callback(jsonError(drogon::k404NotFound, "Tenant not found"));
            co_return;
        }

        if (co_await UserRepository::grantExists(id, tenantId)) {
            callback(jsonError(drogon::k409Conflict,
                "User already has access to this tenant"));
            co_return;
        }

        if (!co_await UserRepository::addGrant(id, tenantId, authUserId(req))) {
            callback(jsonError(drogon::k500InternalServerError,
                "Failed to add grant"));
            co_return;
        }

        auto grants = co_await UserRepository::findGrantsByUserId(id);
        Json::Value response(Json::arrayValue);
        for (const auto& g : grants)
            response.append(grantToJson(g));

        callback(jsonCreated(response));
    }

    // DELETE /api/management/users/:id/grants/:tenantId
    drogon::Task<> UserController::removeGrant(
        drogon::HttpRequestPtr req,
        std::function<void(const drogon::HttpResponsePtr&)> callback,
        std::string id,
        std::string tenantId)
    {
        if (!isCoreAdmin(req)) {
            callback(jsonError(drogon::k403Forbidden,
                "Only CORE_ADMIN can revoke tenant access"));
            co_return;
        }

        if (!co_await UserRepository::grantExists(id, tenantId)) {
            callback(jsonError(drogon::k404NotFound,
                "Grant not found"));
            co_return;
        }

        if (!co_await UserRepository::removeGrant(id, tenantId)) {
            callback(jsonError(drogon::k500InternalServerError,
                "Failed to remove grant"));
            co_return;
        }

        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::k204NoContent);
        callback(resp);
    }

} // namespace SMPPlusPlus
