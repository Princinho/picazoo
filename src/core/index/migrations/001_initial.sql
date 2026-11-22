-- Picazoo — library.db, schéma version 1 (fonctionnalité 001)
-- Voir specs/001-bibliotheque-visionneuse/data-model.md

CREATE TABLE schema_version (
    version INTEGER NOT NULL
);

CREATE TABLE watched_root (
    id            INTEGER PRIMARY KEY,
    path          TEXT    NOT NULL UNIQUE COLLATE NOCASE,
    volume_serial INTEGER,
    state         TEXT    NOT NULL DEFAULT 'available'
                  CHECK (state IN ('available', 'unavailable', 'denied')),
    last_scan_at  INTEGER,
    added_at      INTEGER NOT NULL
);

CREATE TABLE excluded_path (
    id      INTEGER PRIMARY KEY,
    root_id INTEGER NOT NULL REFERENCES watched_root(id) ON DELETE CASCADE,
    path    TEXT    NOT NULL COLLATE NOCASE,
    UNIQUE (root_id, path)
);

CREATE TABLE folder (
    id          INTEGER PRIMARY KEY,
    root_id     INTEGER NOT NULL REFERENCES watched_root(id) ON DELETE CASCADE,
    path        TEXT    NOT NULL UNIQUE COLLATE NOCASE,
    parent_path TEXT    NOT NULL COLLATE NOCASE,
    name        TEXT    NOT NULL,
    item_count  INTEGER NOT NULL DEFAULT 0,
    date        INTEGER
);

CREATE TABLE item (
    id            INTEGER PRIMARY KEY,
    folder_id     INTEGER NOT NULL REFERENCES folder(id) ON DELETE CASCADE,
    file_name     TEXT    NOT NULL COLLATE NOCASE,
    kind          TEXT    NOT NULL CHECK (kind IN ('image', 'raw', 'video')),
    format        TEXT    NOT NULL,
    size_bytes    INTEGER NOT NULL,
    mtime         INTEGER NOT NULL,
    volume_serial INTEGER,
    file_id       BLOB,
    quick_hash    INTEGER,
    taken_at      INTEGER,
    sort_date     INTEGER NOT NULL,
    width         INTEGER,
    height        INTEGER,
    orientation   INTEGER NOT NULL DEFAULT 1,
    camera        TEXT,
    duration_ms   INTEGER,
    meta_state    TEXT    NOT NULL DEFAULT 'pending'
                  CHECK (meta_state IN ('pending', 'done', 'failed')),
    thumb_state   TEXT    NOT NULL DEFAULT 'pending'
                  CHECK (thumb_state IN ('pending', 'provisional', 'done', 'unreadable', 'no_preview')),
    version       INTEGER NOT NULL DEFAULT 1,
    last_seen_at  INTEGER NOT NULL,
    UNIQUE (folder_id, file_name)
);

CREATE INDEX item_folder_sort_date ON item (folder_id, sort_date);
CREATE INDEX item_folder_file_name ON item (folder_id, file_name);
CREATE INDEX item_file_id          ON item (volume_serial, file_id);
CREATE INDEX item_quick_hash       ON item (quick_hash, size_bytes);
CREATE INDEX item_sort_date        ON item (sort_date);
CREATE INDEX item_thumb_state      ON item (thumb_state);

CREATE TABLE settings (
    key   TEXT PRIMARY KEY,
    value TEXT
);

INSERT INTO schema_version (version) VALUES (1);
