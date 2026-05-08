-- Van_Nueman PostgreSQL Schema (from FULL_ARCHITECTURE.md)
-- Players table
CREATE TABLE IF NOT EXISTS players (
    id SERIAL PRIMARY KEY,
    username VARCHAR(64) NOT NULL UNIQUE,
    server_id INTEGER,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    last_login TIMESTAMP,
    tutorial_completed BOOLEAN DEFAULT FALSE,
    genome_count INTEGER DEFAULT 0,
    system_count INTEGER DEFAULT 0
);

-- World persistence (64³ chunk hybrid, from architecture)
CREATE TABLE IF NOT EXISTS world_snapshots (
    server_id INTEGER NOT NULL,
    timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    checksum VARCHAR(64),
    snapshot_data BYTEA,
    PRIMARY KEY (server_id, timestamp)
);

-- SVO chunks (64³ voxels per chunk)
CREATE TABLE IF NOT EXISTS svo_chunks (
    server_id INTEGER NOT NULL,
    x INTEGER NOT NULL,
    y INTEGER NOT NULL,
    z INTEGER NOT NULL,
    level INTEGER NOT NULL DEFAULT 0,
    voxel_data BYTEA NOT NULL,
    last_modified TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (server_id, x, y, z, level)
);

-- Server registry
CREATE TABLE IF NOT EXISTS servers (
    id SERIAL PRIMARY KEY,
    owner_id INTEGER REFERENCES players(id),
    name VARCHAR(128) NOT NULL,
    status VARCHAR(32) DEFAULT 'offline',  -- online, offline, maintenance
    pillar_state BYTEA,  -- Serialized 14-pillar state vector (scaled integers)
    approved BOOLEAN DEFAULT FALSE,
    approved_by INTEGER REFERENCES players(id),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    max_players INTEGER DEFAULT 128,
    current_players INTEGER DEFAULT 0,
    ip_address VARCHAR(45),
    port INTEGER DEFAULT 7777
);

-- Server relationships (federation links)
CREATE TABLE IF NOT EXISTS server_relationships (
    id SERIAL PRIMARY KEY,
    server_a INTEGER REFERENCES servers(id),
    server_b INTEGER REFERENCES servers(id),
    connection_type VARCHAR(32) DEFAULT 'federation',  -- federation, ally, neutral, hostile
    bandwidth FLOAT DEFAULT 100.0,  -- Mbps
    latency FLOAT DEFAULT 50.0,  -- ms
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UNIQUE(server_a, server_b)
);

-- Federation feedback logs (log-decay feedback loops)
CREATE TABLE IF NOT EXISTS feedback_logs (
    id SERIAL PRIMARY KEY,
    source_server INTEGER REFERENCES servers(id),
    target_server INTEGER REFERENCES servers(id),
    force_type VARCHAR(32),  -- Economic, Military, Social, Diplomatic, Cultural, Information
    magnitude FLOAT,
    pillar INTEGER,  -- Which pillar was affected (0-13)
    timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Interaction history (for neuroevolution fitness)
CREATE TABLE IF NOT EXISTS interaction_history (
    id SERIAL PRIMARY KEY,
    server_a INTEGER REFERENCES servers(id),
    server_b INTEGER REFERENCES servers(id),
    outcome VARCHAR(32),  -- success, failure, neutral
    fitness_score FLOAT,
    timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Neuroevolution weights (server PSV population)
CREATE TABLE IF NOT EXISTS evolution_weights (
    id SERIAL PRIMARY KEY,
    server_id INTEGER REFERENCES servers(id),
    pillar_pair VARCHAR(64),  -- e.g., "Force_Resistance"
    weight FLOAT NOT NULL,
    fitness FLOAT,
    generation INTEGER DEFAULT 0,
    timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Operator boundaries (operator-set constraints for neuroevolution)
CREATE TABLE IF NOT EXISTS operator_boundaries (
    id SERIAL PRIMARY KEY,
    server_id INTEGER REFERENCES servers(id),
    pillar INTEGER NOT NULL,  -- 0-13
    min_val FLOAT NOT NULL,
    max_val FLOAT NOT NULL,
    set_by INTEGER REFERENCES players(id),
    timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UNIQUE(server_id, pillar)
);

-- Single player tutorial progress
CREATE TABLE IF NOT EXISTS tutorial_progress (
    id SERIAL PRIMARY KEY,
    player_id INTEGER REFERENCES players(id),
    step INTEGER NOT NULL,
    completed BOOLEAN DEFAULT FALSE,
    completed_at TIMESTAMP,
    UNIQUE(player_id, step)
);

-- CRISPR vault (genome storage)
CREATE TABLE IF NOT EXISTS crispr_vault (
    id SERIAL PRIMARY KEY,
    owner_id INTEGER REFERENCES players(id),
    species_name VARCHAR(128),
    genome_data BYTEA NOT NULL,
    discovered_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    is_shared BOOLEAN DEFAULT FALSE,
    parent_genome INTEGER REFERENCES crispr_vault(id)
);

-- Star system claims
CREATE TABLE IF NOT EXISTS system_claims (
    id SERIAL PRIMARY KEY,
    server_id INTEGER REFERENCES servers(id),
    claimant_id INTEGER REFERENCES players(id),
    star_system_name VARCHAR(128),
    claim_strength FLOAT DEFAULT 1.0,  -- Based on Influence + Presence pillars
    claimed_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UNIQUE(server_id, star_system_name)
);

-- Relay stations (player-built waypoints)
CREATE TABLE IF NOT EXISTS relay_stations (
    id SERIAL PRIMARY KEY,
    owner_id INTEGER REFERENCES players(id),
    server_id INTEGER REFERENCES servers(id),
    star_system VARCHAR(128),
    maintenance_cost FLOAT DEFAULT 10.0,
    trade_income FLOAT DEFAULT 0.0,
    influence_boost FLOAT DEFAULT 5.0,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Indexes for performance
CREATE INDEX idx_players_server ON players(server_id);
CREATE INDEX idx_svo_chunks_server ON svo_chunks(server_id);
CREATE INDEX idx_feedback_logs_servers ON feedback_logs(source_server, target_server);
CREATE INDEX idx_evolution_weights_server ON evolution_weights(server_id);
CREATE INDEX idx_system_claims_server ON system_claims(server_id);
CREATE INDEX idx_crispr_vault_owner ON crispr_vault(owner_id);

-- Initial data: Create federation server
INSERT INTO servers (id, name, status, approved, max_players) 
VALUES (1, 'Federation Hub', 'online', TRUE, 128)
ON CONFLICT (id) DO NOTHING;

-- Initial operator boundaries for server 1 (allow full range with slight constraints)
INSERT INTO operator_boundaries (server_id, pillar, min_val, max_val)
SELECT 1, p, -10.0, 10.0
FROM generate_series(0, 13) AS p
ON CONFLICT (server_id, pillar) DO NOTHING;
