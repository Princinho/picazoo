-- Picazoo — library.db, schéma version 4 (fonctionnalité 007 : mise à la corbeille hors doublons)
-- Migration non destructive : une colonne ajoutée ; les opérations existantes viennent des doublons.
-- Voir specs/007-clic-droit-zoom-plein-ecran/data-model.md

ALTER TABLE file_operation ADD COLUMN source TEXT NOT NULL DEFAULT 'duplicates'
    CHECK (source IN ('duplicates', 'library', 'viewer'));

UPDATE schema_version SET version = 4;
