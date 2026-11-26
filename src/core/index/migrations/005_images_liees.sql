-- Picazoo — library.db, schéma version 5 (fonctionnalité 003 : images liées)
-- Migration non destructive : aucune table de la 001/002/006/007 n'est modifiée.
-- Voir specs/003-images-liees/data-model.md

CREATE TABLE naming_group (
    id            INTEGER PRIMARY KEY,
    root_key      TEXT    NOT NULL UNIQUE COLLATE NOCASE,
    display_name  TEXT    NOT NULL,
    confidence    TEXT    NOT NULL
                  CHECK (confidence IN ('regular_series', 'same_root', 'generic_root')),
    member_count  INTEGER NOT NULL CHECK (member_count >= 2),
    state         TEXT    NOT NULL DEFAULT 'to_review'
                  CHECK (state IN ('to_review', 'confirmed', 'rejected')),
    renamed       INTEGER NOT NULL DEFAULT 0 CHECK (renamed IN (0, 1)),
    updated_at    INTEGER NOT NULL
);

CREATE INDEX naming_group_confidence ON naming_group (confidence);
CREATE INDEX naming_group_state      ON naming_group (state);

CREATE TABLE naming_member (
    group_id      INTEGER NOT NULL REFERENCES naming_group(id) ON DELETE CASCADE,
    item_id       INTEGER NOT NULL UNIQUE REFERENCES item(id) ON DELETE CASCADE,
    detected_root TEXT    NOT NULL,
    manual        INTEGER NOT NULL DEFAULT 0 CHECK (manual IN (0, 1)),
    PRIMARY KEY (group_id, item_id)
);

CREATE TABLE naming_decision (
    root_key        TEXT    PRIMARY KEY COLLATE NOCASE,
    kind            TEXT    NOT NULL CHECK (kind IN ('rejected', 'merged_into')),
    target_root_key TEXT,
    label           TEXT    NOT NULL DEFAULT '', -- nom du groupe au moment de la décision
    decided_at      INTEGER NOT NULL
);

-- Élément retiré d'un groupe par l'utilisateur : un repérage ne le remet pas sous cette racine.
-- `rejected_root` : retrait dû à « Ce n'est pas un groupe », levé quand ce rejet est annulé.
CREATE TABLE naming_exclusion (
    item_id       INTEGER NOT NULL REFERENCES item(id) ON DELETE CASCADE,
    root_key      TEXT    NOT NULL COLLATE NOCASE,
    rejected_root TEXT    COLLATE NOCASE,
    PRIMARY KEY (item_id, root_key)
);

UPDATE schema_version SET version = 5;
