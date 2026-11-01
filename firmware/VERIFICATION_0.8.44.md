# Vérification 0.8.44

Compilation complète réussie avec Arduino M5Stack3.3.9 / ESP-IDF5.5.4 / GCC RISC-V14.2. Sources et copies Arduino vérifiées ; ELF32 little-endian RISC-V, ABI simple précision, image ESP32-P4, checksum et SHA embarqué validés. **Aucun essai sur Tab5 physique.**

| Élément | Résultat |
|---|---|
| Firmware OTA | [Tab5OS_v0.8.44.bin](firmware/Tab5OS_v0.8.44.bin) |
| Taille | 6065728 octets |
| Partition | 6553600 octets |
| Marge | 487872 octets |
| SHA-256 | `c1d493d821b86925146fa253c936ab3907eddf1c47b27039a722e9fa5afdd92a` |
| Sources figées | 2024 fichiers contrôlés |
| Objets de la compilation | 1423 |
| Rapport | [firmware-verification-0844.json](tools/verification/firmware-verification-0844.json) |

La table de partitions est identique à la référence 0.8.43. Les 23 archives de dépendances et Arduino CLI sont revérifiés par SHA-256. Le rapport contrôle aussi les backports crypto, l’allocateur Bluetooth, le chemin d’interruption DSI en IRAM, les compteurs internes et les correctifs JPEG/LVGL réellement liés. Les manifestes et commandes sont joints dans `tools/verification/build-0844`.

## Exécutions ciblées actuelles

| Périmètre | Résultat | Preuve |
|---|---|---|
| audio lifecycle | PASS | [audio-lifecycle-0844.json](tools/verification/audio-lifecycle-0844.json) |
| audio media | PASS | [audio-media-0844.json](tools/verification/audio-media-0844.json) |
| display media | PASS | [display-media-0844.json](tools/verification/display-media-0844.json) |
| map routing | PASS | [map-routing-0844.json](tools/verification/map-routing-0844.json) |
| maps | PASS | [maps-0844.json](tools/verification/maps-0844.json) |
| maps regression | PASS | [maps-regression-0844.json](tools/verification/maps-regression-0844.json) |
| maps target | PASS | [maps-target-0844.json](tools/verification/maps-target-0844.json) |
| media list | PASS | [media-list-0844.json](tools/verification/media-list-0844.json) |
| media ui | PASS | [media-ui-0844.json](tools/verification/media-ui-0844.json) |
| media ui target | PASS | [media-ui-target-0844.json](tools/verification/media-ui-target-0844.json) |
| network | PASS | [network-0844.json](tools/verification/network-0844.json) |
| network target | PASS | [network-target-0844.json](tools/verification/network-target-0844.json) |
| storage | PASS | [storage-0844.json](tools/verification/storage-0844.json) |
| storage target | PASS | [storage-target-0844.json](tools/verification/storage-target-0844.json) |
| video input | PASS | [video-input-0844.json](tools/verification/video-input-0844.json) |
| video lifecycle | PASS | [video-lifecycle-0844.json](tools/verification/video-lifecycle-0844.json) |
| video target | PASS | [video-target-0844.json](tools/verification/video-target-0844.json) |

Les rapports donnent les sources exactes, modèles, fautes injectées et limites. Les anciens rapports restent historiques, conservés byte-identiques à la référence ; aucune nouvelle exécution ne leur est attribuée. Le registre `tools/release_scope.json` exige une preuve actuelle pour chaque source de production modifiée. Les tests hôte ne mesurent ni le temps de démarrage, ni le CPU/énergie, ni les périphériques physiques.

[Diagnostic](AUDIT_0.8.44.md) · [Ressources](BILAN_RESSOURCES_0.8.44.md) · [Procédure matériel](TESTS_TAB5_0.8.44.md).
