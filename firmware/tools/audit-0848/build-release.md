# Reprise de compilation 0.8.48

L’archive livrée 0.8.47 est la référence : SHA-256 `77c881f040ffbdc07815f7ab954acbc5107066a8a24fa48701758f088308dec7`. Ses BIN, ELF et MAP ont été revérifiés contre le rapport de livraison avant comparaison. Leur recompilation ou exécution physique n’est pas revendiquée.

L’environnement Linux a été réinstallé dans `build_0848` avec Arduino CLI 1.5.1, M5Stack 3.3.9, IDF 5.5.4, GCC RV32 14.2.0, M5GFX 0.2.30, M5Unified 0.2.23, LVGL 9.6.0, ArduinoJson 7.4.3 et le tag ESP8266Audio 2.4.2 (métadonnée amont 2.4.1). Les 23 archives et le CLI ont tous passé leur contrôle SHA-256.

Les scripts actifs Linux/GCC, Windows/GCC et Windows/Clang lient les deux unités natives DSI/DW-GDMA reconstruites dans un dossier isolé. Les installations SDK/librairies globales restent intactes. Le helper Clang réutilise les drapeaux ABI du SDK et retire seulement les trois options GCC déjà exclues par ce parcours. Windows n’a pas été exécuté ici.

La vérification de l’image contrôle les sections internes, les IRQ réellement allouées avec IRAM, les littéraux et objets du chemin interruptif, le hook sans E/S, les sources et archives effectivement liées, l’image ESP32-P4 et sa place dans les deux partitions OTA inchangées. Les simples macros données au sketch ne pourraient pas corriger les archives natives déjà compilées.

Le rapport `tools/verification/firmware-verification-0848.json` et ses preuves `build-0848` doivent être consultés pour l’état final. La génération préalable de `compile_commands.json` et les compilations ciblées ne constituent pas une livraison de firmware. Aucune mesure physique de scintillement, latence, PSRAM libre ou bruit audio n’est déduite de ces contrôles.
