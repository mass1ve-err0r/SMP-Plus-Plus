-- Migration 003: Move smp_certificates and sml_configurations to instance level
--
-- Both tables were originally scoped to tenants. Since there is one SMP
-- address per deployment, the signing certificate and SML configurations
-- belong to the instance, not to individual tenants.

-- -------------------------------------------------------------------------
-- Recreate smp_certificates at instance level
-- Drop and recreate since tenant_id was NOT NULL - simpler than ALTER
-- -------------------------------------------------------------------------
DROP TABLE IF EXISTS smp_certificates CASCADE;

CREATE TABLE smp_certificates (
                                  id              UUID        NOT NULL DEFAULT gen_random_uuid(),
                                  label           TEXT        NOT NULL,
                                  cert_pem        TEXT        NOT NULL,
                                  private_key_enc TEXT        NOT NULL,
                                  fingerprint     TEXT        NOT NULL,
                                  valid_from      TIMESTAMPTZ,
                                  valid_until     TIMESTAMPTZ,
                                  is_active       BOOLEAN     NOT NULL DEFAULT false,
                                  created_at      TIMESTAMPTZ NOT NULL DEFAULT now(),
                                  created_by      UUID        REFERENCES users(id) ON DELETE SET NULL,
                                  PRIMARY KEY (id)
);

-- Only one certificate can be active at a time
CREATE UNIQUE INDEX idx_smp_certificates_active
    ON smp_certificates (is_active)
    WHERE is_active = true;

-- -------------------------------------------------------------------------
-- Recreate sml_configurations at instance level
-- Drop tenant_id and client_cert_id - the active cert is used implicitly
-- -------------------------------------------------------------------------
DROP TABLE IF EXISTS sml_configurations CASCADE;

CREATE TABLE sml_configurations (
                                    id                  UUID        NOT NULL DEFAULT gen_random_uuid(),
                                    label               TEXT        NOT NULL,
                                    dns_zone            TEXT        NOT NULL,
                                    management_endpoint TEXT        NOT NULL,
                                    smp_id              TEXT        NOT NULL,
                                    is_primary          BOOLEAN     NOT NULL DEFAULT false,
                                    is_enabled          BOOLEAN     NOT NULL DEFAULT true,
                                    auto_register       BOOLEAN     NOT NULL DEFAULT false,
                                    created_at          TIMESTAMPTZ NOT NULL DEFAULT now(),
                                    PRIMARY KEY (id)
);

-- Only one primary SML at a time
CREATE UNIQUE INDEX idx_sml_configurations_primary
    ON sml_configurations (is_primary)
    WHERE is_primary = true;

-- Default well-known SML entries - user updates endpoints as needed
INSERT INTO sml_configurations
(label, dns_zone, management_endpoint, smp_id, is_primary, is_enabled)
VALUES
    (
        'Peppol Test',
        'api.sml.test.tech.peppol.org',
        'https://api.sml.test.tech.peppol.org/edelivery-sml/',
        '',
        true,
        false   -- disabled until user sets their smp_id
    ),
    (
        'Peppol Production',
        'api.sml.prod.tech.peppol.org',
        'https://api.sml.prod.tech.peppol.org/edelivery-sml/',
        '',
        false,
        false   -- disabled until user sets their smp_id
    )
    ON CONFLICT DO NOTHING;