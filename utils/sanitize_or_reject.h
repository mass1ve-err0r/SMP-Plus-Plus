//
// Created by Saadat Baig on 07.07.26.
//
#pragma once

#include "input_sanitizer.h"

#include <drogon/drogon.h>
#include <initializer_list>
#include <string_view>


namespace SMPPlusPlus
{
    /**
     * sanitizeOrReject - Convenience helper for controllers.
     *
     * Validates the request body's JSON payload against the global forbidden
     * character set. If a violation is found, invokes the callback with a
     * 400 Bad Request response describing the field and character, and returns
     * false. Returns true if the payload is clean.
     *
     * Usage in a controller handler:
     *
     *   const auto body = req->getJsonObject();
     *   if (!body) { ...400... }
     *
     *   if (!sanitizeOrReject(*body, callback,
     *       {"cert_pem", "private_key_pem", "ap_certificate_pem"}))
     *   {
     *       co_return;
     *   }
     *
     * The skipKeys are field names whose values should not be sanitizer-checked
     * (PEMs, signatures, base64 blobs). These have their own dedicated validation
     * (CryptoUtils::certFingerprint, etc.) and legitimately contain characters
     * that would otherwise be flagged (though as of writing, the forbidden set
     * does not intersect with valid PEM chars - this is defense-in-depth).
     */
    inline bool sanitizeOrReject(
        const Json::Value& body,
        const std::function<void(const drogon::HttpResponsePtr&)>& callback,
        const std::initializer_list<std::string_view>& skipKeys = {})
    {
        auto violation = InputSanitizer::validateJson(body, skipKeys);
        if (!violation) return true;

        Json::Value err;
        err["error"] = "Field '" + violation->path +
                       "' contains forbidden character '" +
                       violation->forbiddenChar + "'";
        auto resp = drogon::HttpResponse::newHttpJsonResponse(err);
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
        return false;
    }

} // namespace SMPPlusPlus
