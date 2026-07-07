//
// Created by Saadat Baig on 06.07.26.
//
#pragma once

#include <drogon/HttpResponse.h>
#include <json/value.h>

#include <optional>
#include <string>
#include <string_view>


namespace SMPPlusPlus
{
    class JsonUtils {
    public:
        JsonUtils() = delete;

        [[nodiscard]]
        static inline drogon::HttpResponsePtr ok(const Json::Value& body) {
            return json(body, drogon::k200OK);
        }

        [[nodiscard]]
        static inline drogon::HttpResponsePtr created(const Json::Value& body) {
            return json(body, drogon::k201Created);
        }

        [[nodiscard]]
        static inline drogon::HttpResponsePtr noContent() {
            auto response = drogon::HttpResponse::newHttpResponse();
            response->setStatusCode(drogon::k204NoContent);
            return response;
        }

        [[nodiscard]]
        static inline drogon::HttpResponsePtr error(
            drogon::HttpStatusCode status,
            std::string_view message)
        {
            Json::Value body;
            body["error"] = std::string(message);

            auto response = drogon::HttpResponse::newHttpJsonResponse(body);
            response->setStatusCode(status);
            return response;
        }

        [[nodiscard]]
        static inline drogon::HttpResponsePtr json(
            const Json::Value& body,
            drogon::HttpStatusCode status)
        {
            auto response = drogon::HttpResponse::newHttpJsonResponse(body);
            response->setStatusCode(status);
            return response;
        }

        [[nodiscard]]
        static inline bool hasObject(const Json::Value& value) {
            return value.isObject();
        }

        [[nodiscard]]
        static inline bool hasArray(const Json::Value& value, const char* field) {
            return value.isMember(field) && value[field].isArray();
        }

        [[nodiscard]]
        static inline bool hasNonEmptyArray(const Json::Value& value, const char* field) {
            return hasArray(value, field) && !value[field].empty();
        }

        [[nodiscard]]
        static inline bool hasString(const Json::Value& value, const char* field) {
            return value.isMember(field) && value[field].isString();
        }

        [[nodiscard]]
        static inline bool hasNonEmptyString(const Json::Value& value, const char* field) {
            return hasString(value, field) && !value[field].asString().empty();
        }

        [[nodiscard]]
        static inline std::optional<std::string> requiredString(
            const Json::Value& value,
            const char* field)
        {
            if (!hasNonEmptyString(value, field)) {
                return std::nullopt;
            }

            return value[field].asString();
        }

        [[nodiscard]]
        static inline std::string stringOrDefault(
            const Json::Value& value,
            const char* field,
            std::string_view fallback)
        {
            return hasNonEmptyString(value, field)
                ? value[field].asString()
                : std::string(fallback);
        }

        [[nodiscard]]
        static inline std::string stringOrEmpty(
            const Json::Value& value,
            const char* field)
        {
            return hasString(value, field)
                ? value[field].asString()
                : std::string{};
        }

        [[nodiscard]]
        static inline bool boolOrDefault(
            const Json::Value& value,
            const char* field,
            bool fallback = false)
        {
            return value.isMember(field) && value[field].isBool()
                ? value[field].asBool()
                : fallback;
        }

        [[nodiscard]]
        static inline std::string path(
            std::string_view collection,
            std::size_t index,
            std::string_view field)
        {
            return std::string(collection)
                + "["
                + std::to_string(index)
                + "]."
                + std::string(field);
        }

        [[nodiscard]]
        static inline std::string path(
            std::string_view outerCollection,
            std::size_t outerIndex,
            std::string_view innerCollection,
            std::size_t innerIndex,
            std::string_view field)
        {
            return std::string(outerCollection)
                + "["
                + std::to_string(outerIndex)
                + "]."
                + std::string(innerCollection)
                + "["
                + std::to_string(innerIndex)
                + "]."
                + std::string(field);
        }
    };

}
