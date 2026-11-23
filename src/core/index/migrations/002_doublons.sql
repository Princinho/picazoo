-- Picazoo — library.db, schéma version 2 (fonctionnalité 002 : doublons)
-- Migration non destructive : aucune table de la 001 n'est modifiée.
-- Voir specs/002-detection-doublons/data-model.md

CREATE TABLE item_fingerprint (
    item_id       INTEGER PRIMARY KEY REFERENCES item(id) ON DELETE CASCADE,
    version       INTEGER NOT NULL,
    content_state TEXT    NOT NULL DEFAULT 'not_candidate'
                  CHECK (content_state IN ('not_candidate', 'pending', 'done', 'skipped_online', 'failed')),
    content_hash  BLOB,
    visual_state  TEXT    NOT NULL DEFAULT 'pending'
                  CHECK (visual_state IN ('pending', 'done', 'low_detail', 'not_applicable', 'failed')),
    phash_0       INTEGER,
    phash_90      INTEGER,
    phash_180     INTEGER,
    phash_270     INTEGER,
    detail_score  REAL,
    updated_at    INTEGER NOT NULL
);

CREATE INDEX item_fingerprint_hash   ON item_fingerprint (content_hash) WHERE content_hash IS NOT NULL;
CREATE INDEX item_fingerprint_cstate ON item_fingerprint (content_state);
CREATE INDEX item_fingerprint_vstate ON item_fingerprint (visual_state);

CREATE TABLE dup_group (
    id                INTEGER PRIMARY KEY,
    center_item_id    INTEGER NOT NULL REFERENCES item(id) ON DELETE CASCADE,
    keeper_reason     TEXT    NOT NULL
                      CHECK (keeper_reason IN ('raw', 'resolution', 'less_compressed', 'has_exif', 'oldest', 'path')),
    level             TEXT    NOT NULL CHECK (level IN ('identical', 'same_image', 'similar')),
    member_count      INTEGER NOT NULL CHECK (member_count >= 2),
    reclaimable_bytes INTEGER NOT NULL DEFAULT 0,
    state             TEXT    NOT NULL DEFAULT 'to_review'
                      CHECK (state IN ('to_review', 'kept_all', 'resolved')),
    updated_at        INTEGER NOT NULL
);

CREATE INDEX dup_group_state_bytes ON dup_group (state, reclaimable_bytes DESC);
CREATE INDEX dup_group_level       ON dup_group (level);

CREATE TABLE dup_member (
    group_id   INTEGER NOT NULL REFERENCES dup_group(id) ON DELETE CASCADE,
    item_id    INTEGER NOT NULL UNIQUE REFERENCES item(id) ON DELETE CASCADE,
    level      TEXT    NOT NULL CHECK (level IN ('identical', 'same_image', 'similar')),
    similarity REAL,
    rotation   INTEGER NOT NULL DEFAULT 0 CHECK (rotation IN (0, 90, 180, 270)),
    PRIMARY KEY (group_id, item_id)
);

CREATE TABLE dup_choice (
    item_id    INTEGER PRIMARY KEY REFERENCES item(id) ON DELETE CASCADE,
    keep       INTEGER NOT NULL CHECK (keep IN (0, 1)),
    decided_at INTEGER NOT NULL
);

CREATE TABLE dup_decision (
    item_a     INTEGER NOT NULL REFERENCES item(id) ON DELETE CASCADE,
    item_b     INTEGER NOT NULL REFERENCES item(id) ON DELETE CASCADE,
    kind       TEXT    NOT NULL CHECK (kind IN ('not_duplicate', 'keep_all')),
    decided_at INTEGER NOT NULL,
    PRIMARY KEY (item_a, item_b),
    CHECK (item_a < item_b)
);

CREATE TABLE file_operation (
    id          INTEGER PRIMARY KEY,
    kind        TEXT    NOT NULL CHECK (kind IN ('trash', 'restore')),
    undoes_id   INTEGER REFERENCES file_operation(id),
    created_at  INTEGER NOT NULL,
    finished_at INTEGER,
    state       TEXT    NOT NULL DEFAULT 'planned'
                CHECK (state IN ('planned', 'running', 'done', 'partial', 'failed', 'undone', 'partially_undone')),
    file_count  INTEGER NOT NULL DEFAULT 0,
    bytes       INTEGER NOT NULL DEFAULT 0
);

-- item_id sans clé étrangère : l'élément est retiré de l'index après la mise à la corbeille,
-- le journal doit lui survivre.
CREATE TABLE file_operation_entry (
    id            INTEGER PRIMARY KEY,
    operation_id  INTEGER NOT NULL REFERENCES file_operation(id) ON DELETE CASCADE,
    item_id       INTEGER,
    original_path TEXT    NOT NULL,
    size_bytes    INTEGER NOT NULL,
    mtime         INTEGER NOT NULL,
    content_hash  BLOB,
    keeper_path   TEXT,
    result        TEXT    NOT NULL DEFAULT 'pending'
                  CHECK (result IN ('pending', 'trashed', 'skipped_changed', 'skipped_no_recycle_bin',
                                    'skipped_locked', 'skipped_unavailable', 'failed', 'restored',
                                    'restore_missing', 'restore_conflict')),
    message       TEXT
);

CREATE INDEX file_operation_entry_op ON file_operation_entry (operation_id);

UPDATE schema_version SET version = 2;
