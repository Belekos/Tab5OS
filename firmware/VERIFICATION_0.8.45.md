# Vérification 0.8.45

Compilation complète réussie avec Arduino M5Stack3.3.9 / ESP-IDF5.5.4 / GCC RISC-V14.2. Sources et copies Arduino vérifiées ; ELF32 little-endian RISC-V, ABI simple précision, image ESP32-P4, checksum et SHA embarqué validés. **Aucun essai sur Tab5 physique.**

| Élément | Résultat |
|---|---|
| Firmware OTA | [Tab5OS_v0.8.45.bin](firmware/Tab5OS_v0.8.45.bin) |
| Taille | 6067488 octets |
| Partition | 6553600 octets |
| Marge | 486112 octets |
| SHA-256 | `0b732239e9f63673a0d6afd67339206b8891f2f699e1e0dc9c26f7e0b940afbc` |
| Sources figées | 2026 fichiers contrôlés |
| Objets de la compilation | 1423 |
| Rapport | [firmware-verification-0845.json](tools/verification/firmware-verification-0845.json) |

La table de partitions est identique à la référence 0.8.44. Les 23 archives de dépendances et Arduino CLI sont revérifiés par SHA-256. Le rapport contrôle aussi les backports crypto, l’allocateur Bluetooth, le chemin d’interruption DSI en IRAM, les compteurs internes et les correctifs JPEG/LVGL réellement liés. Les manifestes et commandes sont joints dans `tools/verification/build-0845`.

## Exécutions ciblées actuelles

| Périmètre | Résultat | Preuve |
|---|---|---|
| audio driver | PASS | [audio-driver-0845.json](tools/verification/audio-driver-0845.json) |
| audio target | PASS | [audio-target-0845.json](tools/verification/audio-target-0845.json) |
| audio transport | PASS | [audio-transport-0845.json](tools/verification/audio-transport-0845.json) |
| display driver | PASS | [display-driver-0845.json](tools/verification/display-driver-0845.json) |
| display target | PASS | [display-target-0845.json](tools/verification/display-target-0845.json) |
| fs errno | PASS | [fs-errno-0845.json](tools/verification/fs-errno-0845.json) |
| fs prepare | PASS | [fs-prepare-0845.json](tools/verification/fs-prepare-0845.json) |
| maps input | PASS | [maps-input-0845.json](tools/verification/maps-input-0845.json) |
| maps places | PASS | [maps-places-0845.json](tools/verification/maps-places-0845.json) |
| maps target | PASS | [maps-target-0845.json](tools/verification/maps-target-0845.json) |
| maps worker | PASS | [maps-worker-0845.json](tools/verification/maps-worker-0845.json) |
| sd recovery | PASS | [sd-recovery-0845.json](tools/verification/sd-recovery-0845.json) |
| storage policy | PASS | [storage-policy-0845.json](tools/verification/storage-policy-0845.json) |
| storage sdk | PASS | [storage-sdk-0845.json](tools/verification/storage-sdk-0845.json) |
| storage target | PASS | [storage-target-0845.json](tools/verification/storage-target-0845.json) |

Les rapports donnent les sources exactes, modèles, fautes injectées et limites. Les anciens rapports restent historiques, conservés byte-identiques à la référence ; aucune nouvelle exécution ne leur est attribuée. Le registre `tools/release_scope.json` exige une preuve actuelle pour chaque source de production modifiée. Les tests hôte ne mesurent ni le temps de démarrage, ni le CPU/énergie, ni les périphériques physiques.

[Diagnostic](AUDIT_0.8.45.md) · [Ressources](BILAN_RESSOURCES_0.8.45.md) · [Procédure matériel](TESTS_TAB5_0.8.45.md).
