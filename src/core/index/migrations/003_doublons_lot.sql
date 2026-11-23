-- Picazoo — library.db, schéma version 3 (fonctionnalité 006 : gestion des doublons en lot)
-- Migration non destructive : tables ajoutées et colonnes ajoutées, rien n'est reconstruit.
-- Voir specs/006-gestion-doublons-lot/data-model.md

CREATE TABLE dup_folder_rule (
    id         INTEGER PRIMARY KEY,
    path       TEXT    NOT NULL UNIQUE COLLATE NOCASE,
    kind       TEXT    NOT NULL CHECK (kind IN ('prefer', 'avoid')),
    created_at INTEGER NOT NULL
);

CREATE TABLE dup_batch (
    id          INTEGER PRIMARY KEY,
    kind        TEXT    NOT NULL CHECK (kind IN ('reset', 'keep_folder', 'keep_all', 'not_duplicate')),
    label       TEXT    NOT NULL,
    detail      TEXT,
    group_count INTEGER NOT NULL DEFAULT 0,
    created_at  INTEGER NOT NULL,
    undone_at   INTEGER
);

-- item_id sans clé étrangère : l'élément peut avoir été mis à la corbeille depuis.
CREATE TABLE dup_batch_entry (
    batch_id  INTEGER NOT NULL REFERENCES dup_batch(id) ON DELETE CASCADE,
    item_id   INTEGER NOT NULL,
    prev_keep INTEGER CHECK (prev_keep IN (0, 1)),
    new_keep  INTEGER CHECK (new_keep IN (0, 1)),
    PRIMARY KEY (batch_id, item_id)
);

ALTER TABLE dup_member ADD COLUMN suggested_keep INTEGER NOT NULL DEFAULT 0;
ALTER TABLE dup_group ADD COLUMN suggestion_reason TEXT;
ALTER TABLE dup_group ADD COLUMN suggestion_detail TEXT;
ALTER TABLE dup_group ADD COLUMN flags INTEGER NOT NULL DEFAULT 0;
ALTER TABLE dup_decision ADD COLUMN batch_id INTEGER;
ALTER TABLE file_operation ADD COLUMN stopped INTEGER NOT NULL DEFAULT 0;

CREATE INDEX dup_decision_batch ON dup_decision (batch_id) WHERE batch_id IS NOT NULL;

-- Sans règle de dossier, la copie suggérée est le centre (comportement de la 002).
UPDATE dup_member SET suggested_keep = (
    item_id = (SELECT center_item_id FROM dup_group WHERE dup_group.id = dup_member.group_id));

UPDATE schema_version SET version = 3;
