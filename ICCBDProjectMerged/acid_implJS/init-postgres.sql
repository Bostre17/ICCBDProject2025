-- init-postgres.sql
CREATE TABLE IF NOT EXISTS counters (
  name VARCHAR(255) PRIMARY KEY,
  value BIGINT NOT NULL DEFAULT 0
);

-- Inserisce il contatore 'total' se non esiste già
INSERT INTO counters (name, value) VALUES ('total', 0) ON CONFLICT (name) DO NOTHING;