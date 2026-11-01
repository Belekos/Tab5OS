# Vérification 0.8.43

Compilation complète réussie avec Arduino M5Stack3.3.9 / ESP-IDF5.5.4 / GCC RISC-V14.2. Sources et copies Arduino vérifiées ; ELF32 little-endian RISC-V, ABI simple précision, image ESP32-P4, checksum et SHA embarqué validés. **Aucun essai sur Tab5 physique.**

| Élément | Résultat |
|---|---|
| Firmware OTA | [Tab5OS_v0.8.43.bin](firmware/Tab5OS_v0.8.43.bin) |
| Taille | 6062352 octets |
| Partition | 6553600 octets |
| Marge | 491248 octets |
| SHA-256 | `9c8a2869cb88e8cd923737a574cff51c170380a73ac23995fe03bf85837ca158` |
| Sources figées | 2024 fichiers contrôlés |
| Objets de la compilation | 1423 |
| Rapport | [firmware-verification-0843.json](tools/verification/firmware-verification-0843.json) |

La table de partitions est identique à la référence 0.8.42. Les 23 archives de dépendances et Arduino CLI sont revérifiés par SHA-256. Le rapport contrôle aussi les backports crypto, l’allocateur Bluetooth, le chemin d’interruption DSI en IRAM, les compteurs internes et les correctifs JPEG/LVGL réellement liés. Les manifestes et commandes sont joints dans `tools/verification/build-0843`.

## Exécutions ciblées actuelles

| Périmètre | Résultat | Preuve |
|---|---|---|
| audio events | PASS | [audio-events-0843.json](tools/verification/audio-events-0843.json) |
| audio events target | PASS | [audio-events-target-0843.json](tools/verification/audio-events-target-0843.json) |
| catalogue | PASS | [catalogue-0843.json](tools/verification/catalogue-0843.json) |
| catalogue target | PASS | [catalogue-target-0843.json](tools/verification/catalogue-target-0843.json) |
| display | PASS | [display-0843.json](tools/verification/display-0843.json) |
| display target | PASS | [display-target-0843.json](tools/verification/display-target-0843.json) |
| evolutions localization | PASS | [evolutions-localization-0843.json](tools/verification/evolutions-localization-0843.json) |
| home ui | PASS | [home-ui-0843.json](tools/verification/home-ui-0843.json) |
| resources | PASS | [resources-0843.json](tools/verification/resources-0843.json) |
| root modules target | PASS | [root-modules-target-0843.json](tools/verification/root-modules-target-0843.json) |
| ui diagnostics | PASS | [ui-diagnostics-0843.json](tools/verification/ui-diagnostics-0843.json) |
| ui diagnostics target | PASS | [ui-diagnostics-target-0843.json](tools/verification/ui-diagnostics-target-0843.json) |

Les rapports donnent les sources exactes, modèles, fautes injectées et limites. Les anciens rapports restent historiques, conservés byte-identiques à la référence ; aucune nouvelle exécution ne leur est attribuée. Le registre `tools/release_scope.json` exige une preuve actuelle pour chaque source de production modifiée. Les tests hôte ne mesurent ni le temps de démarrage, ni le CPU/énergie, ni les périphériques physiques.

[Diagnostic](AUDIT_0.8.43.md) · [Ressources](BILAN_RESSOURCES_0.8.43.md) · [Procédure matériel](TESTS_TAB5_0.8.43.md).
