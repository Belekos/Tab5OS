# Socle de Tab5OS 0.8.28

La 0.8.28 conserve les versions de bibliothèques système indiquées ci-dessous. Mail, le navigateur Internet, Quake et Duke Nukem 3D sont retirés de la distribution principale. Les sections 0.8.11 à 0.8.15 retracent les versions antérieures ; elles ne décrivent pas le périmètre courant de ces applications.

| Composant | Version utilisée |
|---|---|
| Arduino CLI | 1.5.1 |
| Core M5Stack / ESP-IDF | 3.3.9 / 5.5.4 |
| M5Unified / M5GFX | 0.2.23 / 0.2.30 |
| LVGL | 9.6.0 |
| ArduinoJson | 7.4.3 |
| ESP8266Audio | 2.4.2 |
| MicroPython intégré | 1.27.0 personnalisé |
| Git | libgit2 1.9.7, sources et adaptateur P4 intégrés |
| SSH / SFTP | libssh2 1.11.2_DEV, instantané officiel épinglé `18aebe08962c1878d84c373ad85caff25ec67771` ; mbedTLS du SDK et adaptations documentées |
| Zstandard / WebP | 1.5.7 / 1.6.0 |
| Stockage USB MSC | 1.1.4, sources intégrées et synchronisation SCSI ajoutée |
| Game Boy Color | gnuboy de retro-go ; provenance et empreinte dans le dossier du cœur |

Le terminal utilise les API C/C++ de fichiers et FreeRTOS, sans nouveau runtime. Le thème utilise la même instance LVGL que l’OS et un seul objet de prévisualisation dessiné. Le décodeur ZIM C++ utilise les mêmes bibliothèques XZ/Zstandard avec une nouvelle politique d’allocation. Le réveil utilise la RTC RX8130CE via M5Unified et le pilote audio commun.

Ce tableau décrit le socle retenu pour reconstruire la 0.8.28 ; les résultats courants sont dans [VERIFICATION.md](VERIFICATION.md). Les versions exécutées sont consignées dans [toolchain-versions.json](tools/verification/toolchain-versions.json). Ce n'est pas une affirmation de dernière version disponible sur Internet. Le réglage USB hub spécifique à IDF 5.5.4 et les changements Game Boy de la 0.8.7 sont conservés ; cette livraison ne les revalide pas sur matériel.

Le pilote MSC 1.1.4 est choisi pour son API compatible IDF 5.5.4 et ses correctifs P4/transferts PSRAM. Ce choix ne constitue pas une mise à jour générale vers les versions les plus récentes. Les sources et licences du pilote sont dans `src/usb_msc`.

<!-- build-status -->
**Compilation complète et contrôles de l’image 0.8.28 réussis.** Preuve : [firmware-verification.json](tools/verification/firmware-verification.json).
<!-- /build-status -->

Les nouvelles polices CJK et les dictionnaires de saisie sont des ressources SD, avec leurs licences et empreintes. Les nouveaux diagnostics utilisent la pile réseau déjà intégrée ; aucun moteur VPN ou Tor n’est ajouté.

## Git et SSH

`src/tab5git` et `src/tab5ssh` contiennent les sources, licences et informations de provenance. Les modifications pour le stockage FAT, les budgets mémoire, les délais, l’authentification et l’admission réseau sont explicites. Ces composants ne constituent pas un tunnel VPN ni un client Tor. Voir les tests de protocoles et leurs limites dans VERIFICATION.md, ainsi que LICENCES.md pour les obligations de redistribution.

## Adaptations de compilation

La voie Espressif Clang prépare des copies isolées de M5GFX 0.2.30 et ESP8266Audio avec des corrections C++ ciblées : pointeurs GPIO et conversion de broche dans `common.inl`, conversion de fréquence I2S dans `AudioOutputPDM.cpp`. Les scripts contrôlent les empreintes des fichiers avant modification et consignent les changements. Les bibliothèques d'origine et le SDK restent intacts. Voir COMPILATION.md pour reproduire cette voie et VERIFICATION.md pour son résultat effectif.

Les voies Arduino/GCC et Clang préparent également une copie isolée de LVGL 9.6.0 avec `prepare_lvgl.py` (Python 3 requis). Le correctif de `lv_text_ap.c` contrôle les deux allocations temporaires du moteur arabe : en cas d’échec, il libère les ressources et conserve le texte UTF-8 original. Les empreintes attendues sont vérifiées ; la bibliothèque installée n’est pas modifiée. Les tests hôtes des allocations et des ligatures figurent dans `tools/verification/arabic-0817.json` ; ils ne constituent pas un essai sur écran Tab5.

## 0.8.11

Les versions de bibliothèques de 0.8.10 restent figées. Le lecteur Zstandard utilise l’API de décompression par blocs de la version 1.5.7 déjà embarquée. Le support manette lit les descripteurs HID sur l’hôte USB existant ; aucun deuxième pilote hôte n’est installé. Les informations de protocole ont été confrontées à HID Usage Tables 1.4 (USB-IF) et aux API USB Host ESP-IDF 5.5 pour ESP32-P4.

- https://www.usb.org/sites/default/files/hut1_4.pdf
- https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32p4/api-reference/peripherals/usb_host.html
- Archive utilisée pour Sodium : https://download.kiwix.org/zim/wikipedia/wikipedia_fr_all_nopic_2026-05.zim

## 0.8.12

Versions conservées. Les corrections concernent le routage USB HID multi-interface et les reprises HTTP des archives. Le coffre utilise les primitives mbedTLS déjà fournies par ESP-IDF : AES-GCM, PBKDF2-HMAC-SHA-256 et CTR-DRBG, amorcé avec l’entropie matérielle au démarrage. Aucune seconde instance LVGL ni nouveau runtime n’est ajouté. Voir SECURITE.md pour les limites de protection.

## Historique : composants ajoutés en 0.8.13

| Composant | Révision utilisée | Usage / adaptations |
|---|---|---|
| ReadyMail | 0.4.2, `c83d19738903bcf80136182762874d1831bee599` | IMAP/SMTP, TLS injecté par Tab5OS, limites de réception et annulation, includes locaux, effacement des chaînes de connexion à la destruction. Licence MIT. |
| retro-go Nofrendo / SMSPlus | `4ced120669750ca7228fd0414211430c1d923166` | ELF NES/Sega8 ; API vidéo/son/stockage Tab5OS, arène PSRAM, CRC et états bornés. Types fixes, palette SMS et lecture NES sans accès désaligné. Licences fournies avec les cœurs. Aucun BIOS Coleco livré. |
| Osmocom rtl-sdr | `797f8143266d983c56d8f35d2d442527529dd8a5` | Contrôle RTL/tuners, adaptateur libusb minimal sur l’hôte ESP-IDF partagé, transferts synchronisés via callbacks. API asynchrone libusb retirée de ce port. GPL-2.0. |
| nanoprintf | v0.6.0 | Formatage dans le runtime ELF, sans flottants ni %n. Licence embarquée dans le header. |

Les identifiants ci-dessus figent les sources réellement embarquées ; ils ne promettent pas que toute version amont plus récente soit meilleure ou compatible. Pas de mise à jour générale de LVGL, M5GFX, M5Unified, ESP-IDF ou MicroPython. Aucun deuxième LVGL.

Sources : https://github.com/mobizt/ReadyMail ; https://github.com/ducalex/retro-go ; https://github.com/osmocom/rtl-sdr ; https://github.com/charlesnicholson/nanoprintf . Les fichiers UPSTREAM.txt donnent les révisions et empreintes des archives utilisées.

## Historique : composants de la 0.8.14

- litehtml 0.10, commit `9bc84b8b8d15a4e50f18b327aa30955048b441c2`, HTML/CSS intégré à LVGL existant. Gumbo embarqué, limites de parsing et de DOM, sortie propre sur épuisement du budget et tables d’encodage constantes en flash. Aucune deuxième instance LVGL.
- gpSP / Snes9x depuis gba-p4, commit `03aeeb07d026fe3138e8cf906e42fb14d00e48b7` ; Gwenesis, PrBoom, Quake et Duke3D/BUILD depuis RetroESP32-P4, commit `339f17ff74eea4fe250d749fdcf0b5e544519c2a`. Adaptateurs SDK Tab5, PSRAM, entrées, son et sauvegardes ; révisions et licences dans chaque dossier.
- Runtime de jeux : formatage flottant activé uniquement pour les trois moteurs 3D. Les bibliothèques et la configuration de l’affichage stable restent inchangées.

Sources : https://github.com/litehtml/litehtml ; https://github.com/x213212/gba-p4 ; voir UPSTREAM.txt pour le dépôt exact des ports RetroESP32-P4.

## Historique : vérification des dépendances 0.8.15

La compilation utilise des copies isolées : M5Unified 0.2.23 et ESP8266Audio tag 2.4.2 ont été récupérés aux versions officielles et comparés aux empreintes Git ; M5GFX 0.2.30, LVGL 9.6.0 et ArduinoJson 7.4.3 proviennent des archives officielles vérifiées. Les bibliothèques Arduino installées chez l’utilisateur n’ont pas été modifiées. La configuration lv_conf.h du projet est explicitement sélectionnée.

Le correctif 0.8.15 n'ajoutait pas de moteur de jeux ni de nouvelle version de LVGL ou de pilote d'affichage. Les preuves de cette ancienne compilation ne valident pas la 0.8.19 ; voir VERIFICATION.md pour le résultat courant et les limites des essais.

## Évolutions 0.8.39

Les versions épinglées du core M5Stack, d’ESP-IDF, de LVGL et des bibliothèques existantes restent inchangées. Le client Meshtastic utilise le Bluetooth/NimBLE fourni par ce SDK, sans ajouter un second hôte Bluetooth externe ni recopier une application Meshtastic. Son adaptateur de protocole est écrit dans le projet ; les références des interfaces publiques sont consignées dans `EVOLUTIONS_0.8.39.md`.

Le formatage utilise FatFs et le pilote MSC déjà présents. Le transport IPP-over-USB utilise l’hôte USB partagé ; il n’installe pas une seconde pile USB. Le choix de polices réutilise le moteur TrueType de LVGL et ses secours multilingues existants. TCC, les outils Flipper Zero et Bitchat sont décrits comme des développements à réaliser dans l’étude jointe.

## Ressources et essais 0.8.40

Les versions de compilation restent celles de la 0.8.39. La recherche hors ligne
ajoute un index **GeoNames cities1000** sur la carte SD, sous licence CC BY 4.0,
avec attribution et empreintes dans `System/maps/gazetteer`. Ce jeu de données
n’est pas une bibliothèque liée au firmware et n’est pas chargé au démarrage.

Les essais de formatage incluent les sources FatFs de l’ESP-IDF **v5.5.4** utilisé,
sous `tools/usb-format-tests-0840/upstream`, pour exécuter de vrais cycles de
formatage et montage sur disque simulé. Les URLs, notices et SHA-256 sont
conservés. Ces sources hôtes ne remplacent pas le pilote fourni par le SDK cible.
Les 26 composants et groupes de ressources recensés figurent dans
`LICENCES-manifest.json` ; cet inventaire ne change pas les licences amont.

## Correctif audio 0.8.47

M5Unified reste en version 0.2.23, sous sa licence MIT. Le script `tools/build_linux/prepare_m5unified_audio.py` prépare une copie isolée et vérifie trois empreintes amont avant modification. Le mode optionnel Tab5 établit les horloges et le transfert silencieux I2S avant l’activation du codec ; les autres configurations conservent leur chemin initial. `verify_m5unified_audio.py` contrôle la copie et sa sélection dans les objets et l’ELF final. Les installations de bibliothèques et le SDK restent intacts. Cette provenance ne remplace pas la validation sonore sur appareil.

## Mémoire et arrêt audio 0.8.49

LVGL 9.6.0 conserve son allocateur TLSF natif derrière un adaptateur de pools PSRAM extensibles ; les pools entièrement libres sont rendus au SDK. M5Unified 0.2.23 reçoit une primitive `stopAndWait` : le producteur attend la restitution effective des références PCM avant de réutiliser les buffers, sans détruire le transport pendant une transition immédiate. La fermeture finale conserve l’arrêt du matériel. Les sources amont restent inchangées dans l’installation ; les scripts fabriquent des copies distinctes vérifiées. Voir le rapport d’ingénierie et les preuves de compilation.
