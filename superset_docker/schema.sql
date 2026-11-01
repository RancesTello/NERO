-- =====================================================================
-- NERO: esquema PostgreSQL de las tablas de telemetría
-- Extraído con pg_dump de la base "superset" (contenedor project-nero-postgres).
-- Solo estructura, sin datos.
--
-- Uso:
--   docker exec -i project-nero-postgres sh -c 'psql -U "$POSTGRES_USER" -d superset' < schema.sql
-- =====================================================================

-- Lecturas EEG (NeuroSky MindWave). Superset la usa como dataset del dashboard "NERO".
CREATE TABLE IF NOT EXISTS public.eeg_data (
    id          SERIAL PRIMARY KEY,
    theta       INTEGER,
    alpha       INTEGER,
    delta       INTEGER,
    beta        INTEGER,
    gamma       INTEGER,
    attention   INTEGER,   -- eSense attention (0-100)
    meditation  INTEGER,   -- eSense meditation (0-100)
    "timestamp" TIMESTAMP WITHOUT TIME ZONE DEFAULT CURRENT_TIMESTAMP
);

-- Tabla de prueba de conectividad (valores de texto con marca de tiempo).
CREATE TABLE IF NOT EXISTS public.datos_sensor (
    id          SERIAL PRIMARY KEY,
    valor       TEXT,
    "timestamp" TIMESTAMP WITHOUT TIME ZONE DEFAULT CURRENT_TIMESTAMP
);
