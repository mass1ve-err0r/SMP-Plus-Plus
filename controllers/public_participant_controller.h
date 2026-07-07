//
// Created by Saadat Baig on 07.07.26.
//
#pragma once

#include <drogon/HttpController.h>


namespace SMPPlusPlus
{
    /**
     * PublicParticipantController - Public SMP directory.
     *
     * GET /participants  - Paginated, deduplicated list of every Peppol
     *                      identifier hosted by this SMP.
     *
     * No authentication. No tenant details leaked. Response is cached at the
     * HTTP layer via Cache-Control: public, max-age=60.
     *
     * NOTE ON ROUTING:
     * This route MUST be registered before the SmpController catch-all
     * "/{participantId}" route, otherwise SmpController would attempt to
     * interpret "participants" as a participant identifier. Drogon's route
     * matcher is order-sensitive within its trie: literal path segments are
     * always matched before parameterized ones at the same depth, so as long
     * as this controller is registered and Drogon sees "/participants" as a
     * literal, no ambiguity exists. Verify with a smoke test after wiring.
     *
     * When the UI presents a row and the user clicks it, the browser navigates
     * to the existing SMP protocol endpoint:
     *   GET /{participantId}          (returns ServiceGroup XML)
     * where {participantId} is the percent-encoded "scheme::value" form.
     */
    class PublicParticipantController
        : public drogon::HttpController<PublicParticipantController> {
    public:
        METHOD_LIST_BEGIN
            ADD_METHOD_TO(PublicParticipantController::listParticipants,
                "/public/participants",
                drogon::Get);
        METHOD_LIST_END

        drogon::Task<> listParticipants(
            drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback
        );
    };

} // namespace SMPPlusPlus
