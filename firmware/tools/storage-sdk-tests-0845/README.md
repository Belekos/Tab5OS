# Régression File/VFS réelle — 0.8.45

`run.py` compile les fichiers FS.cpp, vfs_api.cpp et leurs en-têtes exacts de M5Stack Arduino 3.3.9, les sources VolumeFS/Storage réelles, et la méthode closeOfflinePack réelle extraite de Cartes. Les notices amont et empreintes sont conservées dans `sdk-provenance.json`.

Le SDK n'est pas remplacé par un faux `open` retournant un pointeur nul. Le scénario vérifie précisément qu'un `FileImplPtr` non nul peut représenter un fichier invalide : un champ File persistant faux retenait ainsi une référence au volume dans la référence 0.8.44 et empêchait sa récupération. L'état SDK sous-jacent, les copies de File, les méthodes de lecture/fermeture et les verrous FS sont exécutés réellement. String/Stream, les mutex RTOS et les appels POSIX constituent des frontières adaptées à l'hôte. Le préfixe `/sd` est redirigé vers un répertoire de test, jamais vers une carte utilisateur. L'option `-fpermissive` correspond au SDK Arduino pour sa conversion strchr ; aucune ligne des copies SDK n'est modifiée pour ce test.

```sh
python3 tools/storage-sdk-tests-0845/run.py --baseline-zip /chemin/Tab5OS_0.8.44_Medias_Cartes.zip
python3 tools/storage-sdk-tests-0845/errno_run.py
```

`errno_run.py` applique le backport isolé `prepare_fs_errno.py` à une copie de l'implémentation SDK et confronte les erreurs avant/après. Les fautes malloc/strdup sans errno et les changements d'errno lors de stat/fsync sont des injections destinées à contrôler le contrat d'erreur, pas des observations revendiquées de l'allocateur de la tablette.

Les scénarios incluent descripteur épuisé, chemin absent, allocations refusées, progression de génération, copie/fermeture, 1 000 cycles et 400 accès de métadonnées concurrents. Les fichiers originaux restent intacts. ASan et UBSan sont actifs ; LSan est désactivé car non exploitable dans cet environnement. Des références restantes sont contrôlées explicitement. Ni SPI, ni FatFs embarqué, ni remontage physique ne sont simulés comme réussis.
