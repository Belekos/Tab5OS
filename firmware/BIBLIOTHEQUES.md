# Bibliothèques de Tab5OS 0.8.53

Le [guide de compilation](../docs/BUILD.md) décrit l'installation des dépendances et les commandes. Les versions externes sont épinglées dans [dependency-downloads.json](build/build_linux/dependency-downloads.json).

| Composant | Version utilisée |
| --- | --- |
| Arduino CLI | 1.5.1 |
| Core M5Stack / ESP-IDF | 3.3.9 / 5.5.4 |
| M5Unified / M5GFX | 0.2.23 / 0.2.30 |
| LVGL | 9.6.0 |
| ArduinoJson | 7.4.3 |
| ESP8266Audio | tag 2.4.2 ; métadonnées amont 2.4.1 |
| MicroPython intégré | 1.27.0, port Tab5OS |
| Git | libgit2 1.9.7, adaptateur ESP32-P4 |
| SSH / SFTP | libssh2 1.11.2_DEV, révision `18aebe08962c1878d84c373ad85caff25ec67771` |
| Zstandard / WebP | 1.5.7 / 1.6.0 |
| Stockage USB MSC | 1.1.4, adaptations P4 et synchronisation SCSI |

## Adaptations de compilation

Les scripts de [build/build_linux](build/build_linux/) préparent des copies isolées des dépendances. Ils vérifient les empreintes attendues avant les adaptations LVGL, M5GFX, M5Unified, JPEG, FS, DSI et mémoire. Les bibliothèques installées globalement et le SDK d'origine sont conservés.

LVGL utilise son allocateur TLSF avec des pools PSRAM extensibles. Les adaptations M5Unified coordonnent l'activation audio et la restitution des buffers PCM. Le guide de compilation précise les voies GCC et Clang et leurs limites de validation.

## Sources et ressources intégrées

Les composants embarqués et leurs adaptations se trouvent dans [le sketch](Tab5OS_Arduino_v0.8.53/). Les cœurs des jeux et émulateurs sont conservés sous [tools/native-apps](Tab5OS_Arduino_v0.8.53/tools/native-apps/), avec leurs fichiers de provenance et leurs recettes de compilation.

Les polices, dictionnaires CJK et données cartographiques sont des ressources SD distinctes du firmware. Les générateurs utiles se trouvent dans [build/resources](build/resources/). Le dossier `sdcard` du dépôt ne constitue pas le paquet SD complet.

Les [notices tierces](LICENCES.md) et le [manifeste des licences](LICENCES-manifest.json) recensent les composants et les chemins de leurs textes de licence. Les conditions propres à chaque composant sont conservées dans ses sources ; voir aussi [NOTICE.md](../NOTICE.md).
