# Compilation et intégrité 0.8.44

Référence : ZIP livré 0.8.43, SHA-256 `a0c58c0ba1003722e3f834eac0618dc937c5395afb7cc650dd03220730595e52`, conservé intact. Le manifeste de référence est extrait de ce ZIP ; ses 2 388 fichiers de sketch et 1 881 preuves historiques sont identifiés par SHA-256. Les tests anciens ne sont pas présentés comme réexécutés.

La reconstruction utilise Arduino CLI 1.5.1, M5Stack 3.3.9 / ESP-IDF 5.5.4, GCC RISC-V 14.2.0 (`esp-rv32/2601`), M5Unified 0.2.23, M5GFX 0.2.30, LVGL 9.6.0, ArduinoJson 7.4.3 et le tag ESP8266Audio 2.4.2 dont la métadonnée annonce 2.4.1. Les 23 archives de dépendances et l’archive Arduino CLI ont été revérifiées contre le verrou SHA-256.

Le SDK installé est inchangé. Les correctifs existants LVGL allocation arabe et M5GFX JPEG sont appliqués à des copies isolées et contrôlées par empreinte. Cible conservée : ESP32-P4 `prev3`, PSRAM activée, DIO, partitions par défaut. Le compilateur utilise `F_CPU=360000000L` et les options précédentes ; les scripts de placement mémoire ne sont pas modifiés.

La base de compilation ciblée 0.8.44 contient 1 423 unités ; elle utilise les en-têtes du sketch 0.8.44. Sa génération initiale a eu lieu pendant les éditions : son garde de stabilité l’a donc marquée invalide comme compilation livrable, bien que la base de commandes soit produite. Elle sert uniquement aux contrôles d’objets. Une compilation complète et un manifeste stable sont requis pour le livrable.

La ressource Factory 0.8.44 contient 180 fichiers officiels. Les anciens hashes de ressources restent dans la migration non destructive ; aucun fichier de carte utilisateur n’est touché. Les 40 petits fichiers de récupération embarqués ont été régénérés depuis les sources SD (379 812 octets bruts, 114 420 octets compressés). Le contrôle ressources et la syntaxe des scripts Python sont réussis. Les 14 packs de langues sont identiques à la référence ; leur contrôle couverture, formats et glyphes est réussi.

Six fichiers vides `a-*.su`, restes de compilation à la racine de la livraison, ont été retirés. Aucun code ni donnée utilisateur n’a été supprimé avec eux.

## Compilation finale exécutée

La compilation de développement précédente a bien lié une image, mais des sources avaient changé pendant son exécution : elle a été rejetée par le garde de stabilité et n’est pas livrée. Pour la reconstruction finale, les 563 objets de production et composants vendoriés du sketch ont été supprimés du dossier de travail ; les objets des bibliothèques et du SDK inchangés ont été conservés. Tous les objets du sketch ont donc été recompilés depuis les sources figées.

Compilation finale : 25 octobre 2026, 14:46:44–14:48:48 UTC. Code de sortie 0, sources inchangées, 1 423 objets. Les copies Arduino, le sketch fusionné et les prototypes ajoutés sont vérifiés contre les sources exactes.

L’image et l’ELF passent les contrôles : ELF32 little-endian RISC-V à ABI simple précision ; cible ESP32-P4 ; checksum et hash image valides ; hash ELF embarqué exact ; version 0.8.44 présente. Les backports crypto, les symboles de l’allocateur Bluetooth, le chemin d’interruption DSI en IRAM, les compteurs en RAM interne, les tables constantes en flash et le correctif JPEG réellement compilé sont vérifiés dans le lien final.

| Ressource | 0.8.43 | 0.8.44 | Écart |
|---|---:|---:|---:|
| Image OTA | 6 062 352 | 6 065 728 | +3 376 octets |
| Partition OTA | 6 553 600 | 6 553 600 | inchangée |
| Marge OTA | 491 248 | 487 872 | −3 376 octets |
| Données/BSS internes statiques | 93 252 | 93 268 | +16 octets |
| IRAM texte | 98 494 | 98 494 | inchangé |
| TCM texte/données | 794 / 64 | 794 / 64 | inchangé |
| RTC sans initialisation | 1 200 | 1 200 | inchangée |

SHA-256 du BIN OTA : `c1d493d821b86925146fa253c936ab3907eddf1c47b27039a722e9fa5afdd92a`. La table complète NVS/otadata/app0/app1/SPIFFS/coredump est identique à la référence. Aucune migration de partition n’est requise.

Les preuves exactes se trouvent dans `tools/verification/firmware-verification-0844.json` et `tools/verification/build-0844/` après préparation du livrable. Les anciennes preuves restent historiques. La compilation Windows n’a pas été exécutée. Aucun essai sur Tab5 physique, aucune mesure de heap réel, CPU, démarrage, énergie ou FPS n’est revendiqué.
