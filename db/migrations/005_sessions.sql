-- Migration 005: Session-based authentication

CREATE TABLE IF NOT EXISTS sessions (
                                        id           UUID        NOT NULL DEFAULT gen_random_uuid(),
    token        TEXT        NOT NULL UNIQUE,
    user_id      UUID        NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    created_at   TIMESTAMPTZ NOT NULL DEFAULT now(),
    expires_at   TIMESTAMPTZ NOT NULL,
    last_used_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    user_agent   TEXT,
    ip_address   TEXT,
    PRIMARY KEY (id)
    );

CREATE INDEX IF NOT EXISTS idx_sessions_token   ON sessions (token);
CREATE INDEX IF NOT EXISTS idx_sessions_user_id ON sessions (user_id);
CREATE INDEX IF NOT EXISTS idx_sessions_expires ON sessions (expires_at);
