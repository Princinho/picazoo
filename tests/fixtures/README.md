# Échantillons de test réels

Les tests génèrent eux-mêmes leurs JPEG et PNG (`tests/support/FixtureFactory`).
Certains formats ne peuvent pas être fabriqués par les tests : il faut de
vrais fichiers. Ajoute-les ici, avec **exactement** ces noms. Chacun doit
peser moins de 5 Mo et être libre de droits (tes propres photos conviennent).

| Fichier | Contenu attendu | Utilisé par |
|---|---|---|
| `sample.dng` | photo RAW DNG avec aperçu intégré | US1 (aperçu RAW) |
| `sample.cr2` ou `sample.nef` | photo RAW Canon ou Nikon | US1 (aperçu RAW) |
| `sample.heic` | photo HEIC (iPhone) | US1 (HEIC) |
| `sample.mp4` | vidéo H.264 d'environ 5 s, avec son | US3 (vidéo) |
| `sample.mkv` | vidéo MKV d'environ 5 s | US3 (vidéo) |

Sources possibles (vérifie la licence de chaque fichier) : tes propres
appareils, [raw.pixls.us](https://raw.pixls.us/) (échantillons RAW publiés
sous CC0), les fichiers d'exemple de libheif.

Tant qu'un fichier manque, les tests qui en dépendent sont **ignorés**
(`QSKIP`), et non mis en échec. Ils l'indiquent dans la sortie de `ctest`.

| Fichier | Source | Licence |
|---|---|---|
| _à compléter_ | | |
