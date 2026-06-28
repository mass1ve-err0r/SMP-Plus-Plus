-- Migration 001: Initial schema
-- Creates all base tables for smp++

-- Enable pgcrypto for gen_random_uuid()
CREATE EXTENSION IF NOT EXISTS pgcrypto;

-- -------------------------------------------------------------------------
-- Migration tracking
-- -------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS schema_migrations (
                                                 version     TEXT        NOT NULL,
                                                 applied_at  TIMESTAMPTZ NOT NULL DEFAULT now(),
    PRIMARY KEY (version)
    );

-- -------------------------------------------------------------------------
-- Users
-- role: 'CORE_ADMIN' | 'MANAGEMENT'
-- created_by is NULL for the bootstrap core admin user
-- -------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS users (
                                     id            UUID        NOT NULL DEFAULT gen_random_uuid(),
    username      TEXT        NOT NULL,
    password_hash TEXT        NOT NULL,
    role          TEXT        NOT NULL,
    is_active     BOOLEAN     NOT NULL DEFAULT true,
    created_at    TIMESTAMPTZ NOT NULL DEFAULT now(),
    created_by    UUID        REFERENCES users(id) ON DELETE SET NULL,
    PRIMARY KEY (id),
    UNIQUE (username),
    CONSTRAINT users_role_check CHECK (role IN ('CORE_ADMIN', 'MANAGEMENT'))
    );

-- -------------------------------------------------------------------------
-- Tenants
-- -------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS tenants (
                                       id         UUID        NOT NULL DEFAULT gen_random_uuid(),
    name       TEXT        NOT NULL,
    slug       TEXT        NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    created_by UUID        REFERENCES users(id) ON DELETE SET NULL,
    PRIMARY KEY (id),
    UNIQUE (slug)
    );

-- -------------------------------------------------------------------------
-- User <-> Tenant audit grants
-- Controls which MANAGEMENT users can audit which tenants.
-- CORE_ADMIN bypasses this table entirely (checked at application level).
-- -------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS user_tenant_grants (
                                                  user_id    UUID        NOT NULL REFERENCES users(id)   ON DELETE CASCADE,
    tenant_id  UUID        NOT NULL REFERENCES tenants(id) ON DELETE CASCADE,
    granted_by UUID        REFERENCES users(id)            ON DELETE SET NULL,
    granted_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    PRIMARY KEY (user_id, tenant_id)
    );

-- -------------------------------------------------------------------------
-- SMP signing certificates (one active cert per tenant)
-- private_key_enc: AES-256-GCM blob from CryptoUtils::aesEncrypt()
--   format: base64(iv).base64(tag).base64(ciphertext)
-- -------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS smp_certificates (
                                                id              UUID        NOT NULL DEFAULT gen_random_uuid(),
    tenant_id       UUID        NOT NULL REFERENCES tenants(id) ON DELETE CASCADE,
    label           TEXT        NOT NULL,
    cert_pem        TEXT        NOT NULL,
    private_key_enc TEXT        NOT NULL,
    fingerprint     TEXT        NOT NULL,
    valid_from      TIMESTAMPTZ,
    valid_until     TIMESTAMPTZ,
    created_at      TIMESTAMPTZ NOT NULL DEFAULT now(),
    PRIMARY KEY (id)
    );

CREATE INDEX IF NOT EXISTS idx_smp_certificates_tenant
    ON smp_certificates (tenant_id);

-- -------------------------------------------------------------------------
-- SML configurations (per tenant, multiple allowed, one primary)
-- auto_register: when true, participant creation triggers SML registration.
--   Controlled via system_settings 'sml.auto_register_enabled' globally,
--   and this column per SML config.
-- -------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS sml_configurations (
                                                  id                  UUID        NOT NULL DEFAULT gen_random_uuid(),
    tenant_id           UUID        NOT NULL REFERENCES tenants(id) ON DELETE CASCADE,
    label               TEXT        NOT NULL,
    dns_zone            TEXT        NOT NULL,
    management_endpoint TEXT        NOT NULL,
    smp_id              TEXT        NOT NULL,
    client_cert_id      UUID        REFERENCES smp_certificates(id) ON DELETE SET NULL,
    is_primary          BOOLEAN     NOT NULL DEFAULT false,
    is_enabled          BOOLEAN     NOT NULL DEFAULT true,
    auto_register       BOOLEAN     NOT NULL DEFAULT false,
    created_at          TIMESTAMPTZ NOT NULL DEFAULT now(),
    PRIMARY KEY (id)
    );

CREATE INDEX IF NOT EXISTS idx_sml_configurations_tenant
    ON sml_configurations (tenant_id);

-- Enforce only one primary SML config per tenant
CREATE UNIQUE INDEX IF NOT EXISTS idx_sml_configurations_primary
    ON sml_configurations (tenant_id)
    WHERE is_primary = true;

-- -------------------------------------------------------------------------
-- System settings (global key-value store)
-- Keys are namespaced by convention: "sml.auto_register_enabled", etc.
-- -------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS system_settings (
                                               key        TEXT        NOT NULL,
                                               value      TEXT        NOT NULL,
                                               updated_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    updated_by UUID        REFERENCES users(id) ON DELETE SET NULL,
    PRIMARY KEY (key)
    );

-- Default system settings
INSERT INTO system_settings (key, value) VALUES
                                             ('sml.auto_register_enabled', 'false'),
                                             ('app.version', '1.0.0')
    ON CONFLICT (key) DO NOTHING;
