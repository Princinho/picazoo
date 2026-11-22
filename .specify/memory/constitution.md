<!--
Sync Impact Report
- Version: (template) → 0.1.0 (proposition initiale, non ratifiée)
- Principes ajoutés : I à V
- Sections ajoutées : Contraintes produit, Cycle de travail
- Templates : plan-template.md (Constitution Check) à vérifier lors du premier /speckit-plan
- TODO : ratifier après relecture par le propriétaire du projet
-->

# Picazoo Constitution

Picazoo est un gestionnaire et lecteur de photos et vidéos de bureau, local,
qui reprend l'esprit de Picasa (Google, abandonné en 2016) et y ajoute la
détection des doublons, la détection des images liées et la réorganisation de
la bibliothèque.

## Core Principles

### I. Les fichiers de l'utilisateur sont sacrés (NON-NEGOTIABLE)

- Picazoo ne modifie, ne renomme, ne déplace et ne supprime **jamais** un
  fichier de l'utilisateur sans une action explicite et confirmée de sa part.
- Toute opération qui touche des fichiers (déplacement, suppression,
  réorganisation) montre d'abord un aperçu de ce qui va se passer, écrit un
  journal, et peut être annulée. Une suppression passe par la corbeille du
  système, jamais par un effacement définitif.
- Picazoo n'écrit rien dans les dossiers de l'utilisateur (pas de fichier
  caché à la `.picasa.ini`) : ses données (index, vignettes, groupes,
  réglages) vivent dans son propre espace de stockage.
- Une copie ou un déplacement est vérifié (le fichier d'arrivée est complet et
  identique) avant que l'original ne soit retiré.

### II. Local et privé

Tout fonctionne hors ligne, sur l'ordinateur de l'utilisateur. Aucune photo,
aucune métadonnée et aucune statistique d'usage ne quitte la machine sans une
demande explicite de l'utilisateur.

### III. Fluide sur une vraie bibliothèque

L'application reste réactive avec une bibliothèque de 100 000 photos et
vidéos. Les traitements longs (indexation, vignettes, doublons) tournent en
arrière-plan, montrent leur progression, peuvent être interrompus et
reprennent là où ils s'étaient arrêtés. L'interface n'est jamais bloquée par
eux.

### IV. Le logiciel propose, l'utilisateur décide

Les doublons et les images liées sont des **suggestions** accompagnées d'un
niveau de confiance compréhensible. Picazoo n'agit jamais seul sur la base
d'une estimation ; l'utilisateur valide, corrige ou rejette, et ses choix sont
retenus.

### V. Simple comme Picasa

Une fenêtre, les dossiers à gauche, les vignettes à droite, un double-clic
pour voir en grand. Chaque fonctionnalité nouvelle doit s'intégrer à ce modèle
sans le compliquer ; une option avancée reste cachée tant qu'on n'en a pas
besoin. L'interface est en français.

## Contraintes produit

- Plateforme cible : Windows 10 et 11 en priorité ; les choix techniques ne
  doivent pas fermer la porte à macOS et Linux.
- Formats d'images courants (JPEG, PNG, GIF, BMP, TIFF, WebP, HEIC), photos
  RAW (par leur aperçu intégré) et formats de vidéos courants (MP4, MOV, AVI,
  MKV) pris en charge dès la première version.

## Cycle de travail

- Une fonctionnalité à la fois, dans l'ordre de `docs/feuille-de-route.md`,
  chacune avec sa spec dans `specs/NNN-nom/`.
- Les specs sont rédigées en français.
- Les opérations sur les fichiers (principe I) sont couvertes par des tests
  automatisés sur des dossiers de test, jamais sur de vraies photos.

## Governance

Cette constitution prime sur les autres pratiques du projet. Toute exception
est justifiée dans la section « Complexity Tracking » du plan concerné. Un
changement de principe incrémente la version (MAJEUR : principe retiré ou
redéfini ; MINEUR : principe ajouté ; CORRECTIF : reformulation).

**Version**: 0.1.0 | **Ratified**: TODO (après relecture) | **Last Amended**: 2026-11-22
