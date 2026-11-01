# Compiler Tab5OS 0.8.53

Le sketch est dans [Tab5OS_Arduino_v0.8.53](Tab5OS_Arduino_v0.8.53/), les scripts dans [build](build/). Les commandes ne téléversent aucun appareil.

## Linux / GCC

La [procédure complète](../docs/BUILD.md#linux-x86_64) fournit les dépendances épinglées, les commandes et les vérifications. Elle demande Python 3.10 ou ultérieur, Arduino CLI 1.5.1, le core M5Stack 3.3.9 / ESP-IDF 5.5.4 et GCC RISC-V 14.2.0.

Les bibliothèques sont M5Unified 0.2.23, M5GFX 0.2.30, LVGL 9.6.0, ArduinoJson 7.4.3 et ESP8266Audio tag 2.4.2 (métadonnées 2.4.1). Les [URL et SHA-256](build/build_linux/dependency-downloads.json) sont fournies.

`PartitionScheme=default` sélectionne `tools/partitions/default_16MB.csv` du SDK M5Stack, avec deux emplacements applicatifs de 6 553 600 octets. La cible utilise `ChipVariant=prev3`, `PSRAM=enabled` et `FlashMode=dio`.

## Windows / GCC

Installer Python et Arduino CLI, puis ouvrir PowerShell dans `firmware` :

```powershell
.\Compiler_Tab5OS.ps1 -CheckOnly
.\Compiler_Tab5OS.ps1 -InstallDependencies
```

La première commande vérifie le sketch. La seconde télécharge les dépendances et compile. Ensuite, utiliser `.\Compiler_Tab5OS.ps1` sans `-InstallDependencies`. Le paramètre `-ArduinoCli` accepte le chemin du CLI ; `-SketchPath` sélectionne explicitement le sketch.

Les résultats vont dans `build-environment`. L'installation Arduino globale reste intacte.

## Adaptations et vérification

Les scripts préparent des copies isolées de LVGL, M5GFX, M5Unified et FS, avec vérification des empreintes amont. Ils utilisent le `lv_conf.h` du sketch et reconstruisent les unités natives DSI/DW-GDMA cache-safe. Conserver les options de liaison et le groupe `--whole-archive` générés : une définition ajoutée dans l'IDE ne modifie pas les archives SDK précompilées.

Le vérificateur Linux contrôle les fichiers de la compilation courante, l'image, les partitions et les adaptations liées. `--baseline` permet une comparaison facultative des sections mémoire avec un fichier de référence fourni séparément.

## Windows / Clang

Les [scripts Clang](build/build_clang/README.md) proposent une voie expérimentale. Cette voie n'est pas validée pour la 0.8.53 et ne reproduit pas encore tous les réglages des parcours GCC. Utiliser Linux/GCC pour la reconstruction de référence.

## Applications et ressources

Les [11 exécutables ELF](Tab5OS_Arduino_v0.8.53/sdcard/Applications/) sont fournis avec leurs [sources et recettes](Tab5OS_Arduino_v0.8.53/tools/native-apps/). Ils se recompilent séparément selon leur ABI et leurs dépendances. Les [scripts Python](Tab5OS_Arduino_v0.8.53/sdcard/MicroPython/) sont directement modifiables.

Les en-têtes générés nécessaires au firmware sont inclus. Les [utilitaires de ressources](build/resources/README.md) servent à les régénérer ou à préparer les données externes. Le [paquet SD complet](../downloads/Tab5OS_0.8.53_SD.zip) fournit les applications, les ressources générées et la référence Factory. Le [guide d’installation](../docs/INSTALL_FR.md) décrit l’utilisation des BIN fournis et la préparation de la carte.
