# Vérification 0.8.48

Compilation complète réussie avec Arduino M5Stack3.3.9 / ESP-IDF5.5.4 / GCC RISC-V14.2. Sources et copies Arduino vérifiées ; ELF32 little-endian RISC-V, ABI simple précision, image ESP32-P4, checksum et SHA embarqué validés. **Aucun essai sur Tab5 physique.**

| Élément | Résultat |
|---|---|
| Firmware OTA | [Tab5OS_v0.8.48.bin](firmware/Tab5OS_v0.8.48.bin) |
| Taille | 6084512 octets |
| Partition | 6553600 octets |
| Marge | 469088 octets |
| SHA-256 | `bfb09d79857a6c6bab18ced7726f3bc1c947a1a2fd2633d341731f1c8ce20fc1` |
| Sources figées | 2030 fichiers contrôlés |
| Objets de la compilation | 1423 |
| Rapport | [firmware-verification-0848.json](tools/verification/firmware-verification-0848.json) |

La table de partitions est identique à la référence 0.8.47. Les 23 archives de dépendances et Arduino CLI sont revérifiés par SHA-256. Le rapport contrôle aussi les backports crypto, l’allocateur Bluetooth, le chemin d’interruption DSI en IRAM, les compteurs internes et les correctifs JPEG/LVGL et le démarrage audio M5Unified réellement liés. Les manifestes et commandes sont joints dans `tools/verification/build-0848`.

## Contrôles et exécutions ciblés actuels

| Périmètre | Résultat | Preuve |
|---|---|---|
| audio housekeeping | PASS | [audio-housekeeping-0848.json](tools/verification/audio-housekeeping-0848.json) |
| audio reset | PASS | [audio-reset-0848.json](tools/verification/audio-reset-0848.json) |
| audio sdk target | PASS | [audio-sdk-target-0848.json](tools/verification/audio-sdk-target-0848.json) |
| audio startup regression | PASS | [audio-startup-regression-0848.json](tools/verification/audio-startup-regression-0848.json) |
| compiler warning current | PASS | [compiler-warning-current-0848.json](tools/verification/compiler-warning-current-0848.json) |
| display | PASS | [display-0848.json](tools/verification/display-0848.json) |
| display pending independent | PASS | [display-pending-independent-0848.json](tools/verification/display-pending-independent-0848.json) |
| display signals | PASS | [display-signals-0848.json](tools/verification/display-signals-0848.json) |
| display signals target | PASS | [display-signals-target-0848.json](tools/verification/display-signals-target-0848.json) |
| display target | PASS | [display-target-0848.json](tools/verification/display-target-0848.json) |
| drawing core | PASS | [drawing-core-0848.json](tools/verification/drawing-core-0848.json) |
| drawing target | PASS | [drawing-target-0848.json](tools/verification/drawing-target-0848.json) |
| dsi native | PASS | [dsi-native-0848.json](tools/verification/dsi-native-0848.json) |
| image native | PASS | [image-native-0848.json](tools/verification/image-native-0848.json) |
| independent warning comparison | PASS | [independent-warning-comparison-0848.json](tools/verification/independent-warning-comparison-0848.json) |
| main integration | PASS | [main-integration-0848.json](tools/verification/main-integration-0848.json) |
| media persistence | PASS | [media-persistence-0848.json](tools/verification/media-persistence-0848.json) |
| network gate regression | PASS | [network-gate-regression-0848.json](tools/verification/network-gate-regression-0848.json) |
| release copy filter | PASS | [release-copy-filter-0848.json](tools/verification/release-copy-filter-0848.json) |
| video buffers | PASS | [video-buffers-0848.json](tools/verification/video-buffers-0848.json) |
| video jpeg probe | PASS | [video-jpeg-probe-0848.json](tools/verification/video-jpeg-probe-0848.json) |
| video lifecycle | PASS | [video-lifecycle-0848.json](tools/verification/video-lifecycle-0848.json) |
| video target | PASS | [video-target-0848.json](tools/verification/video-target-0848.json) |

Les rapports donnent les sources exactes, modèles, fautes injectées et limites. Les anciens rapports restent historiques, conservés byte-identiques à la référence ; aucune nouvelle exécution ne leur est attribuée. Le registre `tools/release_scope.json` exige une preuve actuelle pour chaque source de production modifiée. Les tests hôte ne mesurent ni le temps de démarrage, ni le CPU/énergie, ni les périphériques physiques.

[Diagnostic](AUDIT_0.8.48.md) · [Ressources](BILAN_RESSOURCES_0.8.48.md) · [Procédure matériel](TESTS_TAB5_0.8.48.md).
