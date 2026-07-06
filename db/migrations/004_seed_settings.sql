-- Migration 004: Seed additional system settings

INSERT INTO system_settings (key, value) VALUES
    ('smp.id', '')
    ON CONFLICT (key) DO NOTHING;
