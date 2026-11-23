# Picazoo — feuille de route

*Proposition du 2026-11-22, à valider ensemble.*

Picazoo reprend l'esprit de Picasa : un logiciel de bureau, local, qui trouve
tout seul les photos et vidéos de l'ordinateur, les affiche par dossier en
vignettes, et les ouvre en grand en un clic. On y ajoute trois choses que
Picasa ne faisait pas (ou mal) : les doublons, les images liées et la
réorganisation de la bibliothèque.

On avance **une fonctionnalité à la fois**, chacune passant par le cycle
Spec Kit (`specify` → `clarify` → `plan` → `tasks` → `implement`).

| # | Fonctionnalité | Ce que l'utilisateur obtient | Dépend de |
|---|----------------|------------------------------|-----------|
| 001 | **Bibliothèque et visionneuse** | Choisir les dossiers à surveiller, voir toutes ses photos et vidéos par dossier en vignettes, les ouvrir en grand, lire les vidéos, bibliothèque tenue à jour toute seule. | — |
| 002 | **Détection des doublons** | Retrouver les images identiques, même en résolutions, formats ou compressions différents ; choisir lesquelles garder ; mettre les autres à la corbeille. | 001 |
| 003 | **Images liées** | Repérer les images dont les noms partagent une même racine (`vacances_bali_001`, `vacances_bali_002`…), afficher une estimation de confiance, et laisser l'utilisateur valider ou corriger des « groupes ». | 001 |
| 006 | **Gestion des doublons en lot** | Sélectionner des centaines de groupes et agir en une fois, garder ou écarter selon le dossier (paires de dossiers, règles durables), vue compacte, récapitulatif par dossier, arrêt et annulation des lots. | 002 |
| 004 | **Réorganisation de la bibliothèque** | Copier ou déplacer toute la bibliothèque vers un dossier choisi, avec un sous-dossier par groupe d'images liées ; aperçu avant d'agir, journal et annulation. | 001, 003 (002 recommandé avant) |
| 005 | **Visionneuse autonome** | Comme la Visionneuse de photos Picasa : un double-clic sur une photo dans l'Explorateur ouvre seulement une visionneuse légère, quasi instantanée, sans bibliothèque ni analyse ; parcours des images du dossier avec transitions animées, zoom à la molette centré sur le curseur ; association avec Windows proposée, jamais imposée ; passage vers l'application complète. | 001 (peut passer avant 002–004) |

État au 2026-11-23 : 001 livrée ; 002 implémentée (branche `claude/002-detection-doublons-b93eeb`, en revue).

## Pistes pour après (non engagées)

- Étoiles, mots-clés et albums virtuels (sans toucher aux fichiers).
- Retouches simples non destructives (recadrage, redressement, « J'ai de la chance »).
- Recherche par nom, date, mot-clé.
- Vue chronologique.
- Diaporama.
- Reconnaissance des visages et regroupement par personne.
- Carte (photos géolocalisées).
- Export redimensionné, impression, envoi par e-mail.
