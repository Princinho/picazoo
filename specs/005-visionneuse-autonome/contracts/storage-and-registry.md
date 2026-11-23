# Contrat de stockage et de registre

## Écritures sur le disque

`picazoo-viewer.exe` n'écrit **que** :

| Où | Quoi | Quand |
|---|---|---|
| `%LOCALAPPDATA%\Picazoo\viewer.ini` (ou `PICAZOO_DATA_DIR`) | réglages du data-model | quand une valeur change |

Jamais : dans le dossier consulté ni ailleurs sur les disques de
l'utilisateur ; dans `library.db` ou `thumbs.db` ; de cache QML (désactivé) ;
de vignettes ; de journal.

Les fichiers consultés sont ouverts en lecture seule, partage lecture,
écriture et suppression, et refermés après décodage.

Vérification : `tst_folderbrowser` et le scénario manuel 7 du quickstart
comparent un instantané du dossier et du dossier de données avant et après.

## Registre (`--register`, `--unregister`)

Racine `R` = `HKCU\Software\Classes` (ou `HKLM\Software\Classes` avec
`--machine` ; `HKCU\<PICAZOO_REGISTRY_ROOT>\Classes` en test).

| Clé | Valeur |
|---|---|
| `R\Picazoo.Image` | `(défaut)` = « Photo (Picazoo) », `FriendlyTypeName` |
| `R\Picazoo.Image\DefaultIcon` | `"…\picazoo-viewer.exe",0` |
| `R\Picazoo.Image\shell\open\command` | `"…\picazoo-viewer.exe" "%1"` |
| `R\Applications\picazoo-viewer.exe` | `FriendlyAppName` = « Picazoo » |
| `R\Applications\picazoo-viewer.exe\SupportedTypes` | une valeur vide par extension |
| `R\.<ext>\OpenWithProgids` | valeur vide `Picazoo.Image` |
| `…\Software\Picazoo\Capabilities` | `ApplicationName`, `ApplicationDescription` |
| `…\Software\Picazoo\Capabilities\FileAssociations` | `.<ext>` = `Picazoo.Image` |
| `…\Software\RegisteredApplications` | `Picazoo` = `Software\Picazoo\Capabilities` |

Extensions : `.jpg .jpeg .jpe .png .gif .bmp .tif .tiff .webp .heic .heif
.cr2 .cr3 .nef .arw .orf .rw2 .raf .dng` (celles de `media/Formats`).

**Jamais écrits** : la valeur par défaut de `R\.<ext>`, toute clé
`…\FileExts\.<ext>\UserChoice`. `--unregister` retire exactement ce qui est
listé ci-dessus et rien d'autre (une valeur `OpenWithProgids` d'une autre
application n'est pas touchée).
