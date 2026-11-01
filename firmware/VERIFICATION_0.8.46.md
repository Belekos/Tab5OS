# Vérification 0.8.46

Compilation complète réussie avec Arduino M5Stack3.3.9 / ESP-IDF5.5.4 / GCC RISC-V14.2. Sources et copies Arduino vérifiées ; ELF32 little-endian RISC-V, ABI simple précision, image ESP32-P4, checksum et SHA embarqué validés. **Aucun essai sur Tab5 physique.**

| Élément | Résultat |
|---|---|
| Firmware OTA | [Tab5OS_v0.8.46.bin](firmware/Tab5OS_v0.8.46.bin) |
| Taille | 6071360 octets |
| Partition | 6553600 octets |
| Marge | 482240 octets |
| SHA-256 | `adedb383b62bae11b235d72271c8dda6addf8bb3fe04344349150e77ac199443` |
| Sources figées | 2026 fichiers contrôlés |
| Objets de la compilation | 1423 |
| Rapport | [firmware-verification-0846.json](tools/verification/firmware-verification-0846.json) |

La table de partitions est identique à la référence 0.8.45. Les 23 archives de dépendances et Arduino CLI sont revérifiés par SHA-256. Le rapport contrôle aussi les backports crypto, l’allocateur Bluetooth, le chemin d’interruption DSI en IRAM, les compteurs internes et les correctifs JPEG/LVGL réellement liés. Les manifestes et commandes sont joints dans `tools/verification/build-0846`.

## Exécutions ciblées actuelles

| Périmètre | Résultat | Preuve |
|---|---|---|
| audio lifecycle | PASS | [audio-lifecycle-0846.json](tools/verification/audio-lifecycle-0846.json) |
| audio target | PASS | [audio-target-0846.json](tools/verification/audio-target-0846.json) |
| audio transport | PASS | [audio-transport-0846.json](tools/verification/audio-transport-0846.json) |
| audio volume | PASS | [audio-volume-0846.json](tools/verification/audio-volume-0846.json) |
| display | PASS | [display-0846.json](tools/verification/display-0846.json) |
| display driver | PASS | [display-driver-0846.json](tools/verification/display-driver-0846.json) |
| display target | PASS | [display-target-0846.json](tools/verification/display-target-0846.json) |
| drawing core | PASS | [drawing-core-0846.json](tools/verification/drawing-core-0846.json) |
| drawing review | PASS | [drawing-review-0846.json](tools/verification/drawing-review-0846.json) |
| drawing target | PASS | [drawing-target-0846.json](tools/verification/drawing-target-0846.json) |
| image native | PASS | [image-native-0846.json](tools/verification/image-native-0846.json) |
| maps ui | PASS | [maps-ui-0846.json](tools/verification/maps-ui-0846.json) |
| maps worker | PASS | [maps-worker-0846.json](tools/verification/maps-worker-0846.json) |
| media list | PASS | [media-list-0846.json](tools/verification/media-list-0846.json) |
| network gate | PASS | [network-gate-0846.json](tools/verification/network-gate-0846.json) |
| network target | PASS | [network-target-0846.json](tools/verification/network-target-0846.json) |
| serial async | PASS | [serial-async-0846.json](tools/verification/serial-async-0846.json) |
| serial target | PASS | [serial-target-0846.json](tools/verification/serial-target-0846.json) |

Les rapports donnent les sources exactes, modèles, fautes injectées et limites. Les anciens rapports restent historiques, conservés byte-identiques à la référence ; aucune nouvelle exécution ne leur est attribuée. Le registre `tools/release_scope.json` exige une preuve actuelle pour chaque source de production modifiée. Les tests hôte ne mesurent ni le temps de démarrage, ni le CPU/énergie, ni les périphériques physiques.

[Diagnostic](AUDIT_0.8.46.md) · [Ressources](BILAN_RESSOURCES_0.8.46.md) · [Procédure matériel](TESTS_TAB5_0.8.46.md).
