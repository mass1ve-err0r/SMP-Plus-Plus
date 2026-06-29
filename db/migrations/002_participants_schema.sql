-- Migration 002: Participants, ServiceMetadata, Processes, Endpoints

-- -------------------------------------------------------------------------
-- Participants
-- One row per Peppol identifier registered under a tenant.
-- ServiceGroup is implicitly 1:1 with participant - not a separate table.
-- identifier_scheme: e.g. "iso6523-actorid-upis"
-- identifier_value:  e.g. "0088:123456789"
-- -------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS participants (
                                            id                UUID        NOT NULL DEFAULT gen_random_uuid(),
    tenant_id         UUID        NOT NULL REFERENCES tenants(id) ON DELETE CASCADE,
    identifier_scheme TEXT        NOT NULL,
    identifier_value  TEXT        NOT NULL,
    created_at        TIMESTAMPTZ NOT NULL DEFAULT now(),
    created_by        UUID        REFERENCES users(id) ON DELETE SET NULL,
    PRIMARY KEY (id),
    UNIQUE (tenant_id, identifier_scheme, identifier_value)
    );

CREATE INDEX IF NOT EXISTS idx_participants_tenant
    ON participants (tenant_id);

-- -------------------------------------------------------------------------
-- ServiceMetadata
-- One row per document type a participant can receive.
-- Maps to the SMP spec's ServiceMetadata/ServiceInformation element.
-- -------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS service_metadata (
                                                id                    UUID        NOT NULL DEFAULT gen_random_uuid(),
    participant_id        UUID        NOT NULL REFERENCES participants(id) ON DELETE CASCADE,
    document_type_scheme  TEXT        NOT NULL,
    document_type_value   TEXT        NOT NULL,
    created_at            TIMESTAMPTZ NOT NULL DEFAULT now(),
    PRIMARY KEY (id),
    UNIQUE (participant_id, document_type_scheme, document_type_value)
    );

CREATE INDEX IF NOT EXISTS idx_service_metadata_participant
    ON service_metadata (participant_id);

-- -------------------------------------------------------------------------
-- Processes
-- One row per business process under a ServiceMetadata entry.
-- process_scheme: e.g. "cenbii-procid-ubl"
-- process_value:  e.g. "urn:www.cenbii.eu:profile:bii04:ver2.0"
-- -------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS processes (
                                         id                  UUID        NOT NULL DEFAULT gen_random_uuid(),
    service_metadata_id UUID        NOT NULL REFERENCES service_metadata(id) ON DELETE CASCADE,
    process_scheme      TEXT        NOT NULL,
    process_value       TEXT        NOT NULL,
    created_at          TIMESTAMPTZ NOT NULL DEFAULT now(),
    PRIMARY KEY (id),
    UNIQUE (service_metadata_id, process_scheme, process_value)
    );

CREATE INDEX IF NOT EXISTS idx_processes_service_metadata
    ON processes (service_metadata_id);

-- -------------------------------------------------------------------------
-- Endpoints
-- One row per transport profile per process.
-- In practice always one per process (peppol-transport-as4-v2_0).
-- ap_certificate_pem: public cert only, stored plaintext (no private key).
-- -------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS endpoints (
                                         id                        UUID        NOT NULL DEFAULT gen_random_uuid(),
    process_id                UUID        NOT NULL REFERENCES processes(id) ON DELETE CASCADE,
    transport_profile         TEXT        NOT NULL DEFAULT 'peppol-transport-as4-v2_0',
    endpoint_url              TEXT        NOT NULL,
    ap_certificate_pem        TEXT        NOT NULL,
    require_bls               BOOLEAN     NOT NULL DEFAULT false,
    service_activation_date   TIMESTAMPTZ,
    service_expiration_date   TIMESTAMPTZ,
    technical_contact_url     TEXT,
    technical_information_url TEXT,
    service_description       TEXT,
    created_at                TIMESTAMPTZ NOT NULL DEFAULT now(),
    PRIMARY KEY (id),
    -- One endpoint per transport profile per process
    UNIQUE (process_id, transport_profile)
    );

CREATE INDEX IF NOT EXISTS idx_endpoints_process
    ON endpoints (process_id);
