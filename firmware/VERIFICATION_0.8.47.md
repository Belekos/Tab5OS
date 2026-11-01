# Vérification 0.8.47

Compilation complète réussie avec Arduino M5Stack3.3.9 / ESP-IDF5.5.4 / GCC RISC-V14.2. Sources et copies Arduino vérifiées ; ELF32 little-endian RISC-V, ABI simple précision, image ESP32-P4, checksum et SHA embarqué validés. **Aucun essai sur Tab5 physique.**

| Élément | Résultat |
|---|---|
| Firmware OTA | [Tab5OS_v0.8.47.bin](firmware/Tab5OS_v0.8.47.bin) |
| Taille | 6080080 octets |
| Partition | 6553600 octets |
| Marge | 473520 octets |
| SHA-256 | `d0818cd490ad71e6a7c6d2af29b86ab2bedd37a519cfb38b82e2e1d9bd7031bd` |
| Sources figées | 2028 fichiers contrôlés |
| Objets de la compilation | 1423 |
| Rapport | [firmware-verification-0847.json](tools/verification/firmware-verification-0847.json) |

La table de partitions est identique à la référence 0.8.46. Les 23 archives de dépendances et Arduino CLI sont revérifiés par SHA-256. Le rapport contrôle aussi les backports crypto, l’allocateur Bluetooth, le chemin d’interruption DSI en IRAM, les compteurs internes et les correctifs JPEG/LVGL et le démarrage audio M5Unified réellement liés. Les manifestes et commandes sont joints dans `tools/verification/build-0847`.

## Contrôles et exécutions ciblés actuels

| Périmètre | Résultat | Preuve |
|---|---|---|
| audio catalogue | PASS | [audio-catalogue-0847.json](tools/verification/audio-catalogue-0847.json) |
| audio decoder | PASS | [audio-decoder-0847.json](tools/verification/audio-decoder-0847.json) |
| audio driver | PASS | [audio-driver-0847.json](tools/verification/audio-driver-0847.json) |
| audio sdr | PASS | [audio-sdr-0847.json](tools/verification/audio-sdr-0847.json) |
| audio target | PASS | [audio-target-0847.json](tools/verification/audio-target-0847.json) |
| audio transport regression | PASS | [audio-transport-regression-0847.json](tools/verification/audio-transport-regression-0847.json) |
| compiler warning current | PASS | [compiler-warning-current-0847.json](tools/verification/compiler-warning-current-0847.json) |
| compiler warning recorded comparison | PASS | [compiler-warning-recorded-comparison-0847.json](tools/verification/compiler-warning-recorded-comparison-0847.json) |
| display | PASS | [display-0847.json](tools/verification/display-0847.json) |
| display target | PASS | [display-target-0847.json](tools/verification/display-target-0847.json) |
| main integration | PASS | [main-integration-0847.json](tools/verification/main-integration-0847.json) |
| media catalogue target | PASS | [media-catalogue-target-0847.json](tools/verification/media-catalogue-target-0847.json) |
| media independent review | PASS | [media-independent-review-0847.json](tools/verification/media-independent-review-0847.json) |
| media ui target | PASS | [media-ui-target-0847.json](tools/verification/media-ui-target-0847.json) |
| memory | PASS | [memory-0847.json](tools/verification/memory-0847.json) |
| network gate regression | PASS | [network-gate-regression-0847.json](tools/verification/network-gate-regression-0847.json) |
| normalized audio independent review | PASS | [normalized-audio-independent-review-0847.json](tools/verification/normalized-audio-independent-review-0847.json) |
| normalized audio sdk target | PASS | [normalized-audio-sdk-target-0847.json](tools/verification/normalized-audio-sdk-target-0847.json) |
| normalized audio startup | PASS | [normalized-audio-startup-0847.json](tools/verification/normalized-audio-startup-0847.json) |
| normalized display blit independent | PASS | [normalized-display-blit-independent-0847.json](tools/verification/normalized-display-blit-independent-0847.json) |
| normalized flash context | PASS | [normalized-flash-context-0847.json](tools/verification/normalized-flash-context-0847.json) |
| normalized media persistence | PASS | [normalized-media-persistence-0847.json](tools/verification/normalized-media-persistence-0847.json) |
| normalized media ui | PASS | [normalized-media-ui-0847.json](tools/verification/normalized-media-ui-0847.json) |
| normalized media ui size | PASS | [normalized-media-ui-size-0847.json](tools/verification/normalized-media-ui-size-0847.json) |
| normalized resource race independent | PASS | [normalized-resource-race-independent-0847.json](tools/verification/normalized-resource-race-independent-0847.json) |
| resource independent | PASS | [resource-independent-0847.json](tools/verification/resource-independent-0847.json) |
| resource sizes | PASS | [resource-sizes-0847.json](tools/verification/resource-sizes-0847.json) |
| resource target | PASS | [resource-target-0847.json](tools/verification/resource-target-0847.json) |
| writing cache | PASS | [writing-cache-0847.json](tools/verification/writing-cache-0847.json) |

Les rapports donnent les sources exactes, modèles, fautes injectées et limites. Les anciens rapports restent historiques, conservés byte-identiques à la référence ; aucune nouvelle exécution ne leur est attribuée. Le registre `tools/release_scope.json` exige une preuve actuelle pour chaque source de production modifiée. Les tests hôte ne mesurent ni le temps de démarrage, ni le CPU/énergie, ni les périphériques physiques.

[Diagnostic](AUDIT_0.8.47.md) · [Ressources](BILAN_RESSOURCES_0.8.47.md) · [Procédure matériel](TESTS_TAB5_0.8.47.md).
