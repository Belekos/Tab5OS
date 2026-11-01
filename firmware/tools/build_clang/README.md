# Reconstruction avec Espressif Clang

Cette variante de compilation Windows utilise Arduino CLI, Espressif Clang et le linker GNU livré avec Clang. Elle reconstruit les sources complètes sans nécessiter le cache d'une version précédente.

## Dépendances vérifiées

- Python 3.11 et Arduino CLI 1.5.1.
- Core Arduino M5Stack 3.3.9, avec ses outils `esp32p4_es-libs/3.3.9` et `esp-rv32/2601`. Les en-têtes et bibliothèques du runtime GCC sont utilisés.
- Espressif Clang `esp-20.1.1_20250829` et son `riscv32-esp-elf-clang-ld.exe` (GNU Binutils 2.43.1).
- Un dossier contenant M5Unified 0.2.23, M5GFX 0.2.30, LVGL 9.6.0, ArduinoJson 7.4.3 et ESP8266Audio tag 2.4.2. Ce dernier déclare encore la version 2.4.1 dans `library.properties`.

Les paramètres permettent de désigner des installations à d'autres emplacements. Lancer `python compile_tab5_clang.py --help` pour les afficher.

## Commande PowerShell

Depuis le dossier de la livraison, adapter les chemins des dépendances :

```powershell
python .\tools\build_clang\compile_tab5_clang.py `
  --release-root . `
  --libraries-dir C:\workspace\tab5-deps-0815 `
  --work-dir C:\workspace\tab5-build-reproductible `
  --arduino-data "$env:LOCALAPPDATA\Arduino15" `
  --jobs 4
```

Le dossier de travail contient le journal, la commande exacte, les versions des outils, les empreintes des sources avant/après compilation et les binaires sous `build/`. Une modification des sources pendant la compilation fait échouer sa validation. `--mode prepare` écrit uniquement la configuration ; `--mode database` prépare la base des commandes sans compiler les objets.

## Paramètres conservés

La cible reste `m5stack:esp32:m5stack_tab5`, `ChipVariant=prev3`, PSRAM activée, partitions par défaut et flash DIO. L'ABI est `ilp32f` avec `rv32imafc_zicsr_zifencei_xesppie`. Les exceptions C++, les protections de pile et les scripts mémoire du SDK sont conservés. Le linker utilisé est GNU ld, pas LLD.

Trois options propres à GCC sont omises avec Clang : `-freorder-blocks`, `-fstrict-volatile-bitfields` et `-fno-tree-switch-conversion`. Le script génère ces adaptations dans le dossier de travail ; l'installation Arduino reste intacte.

`prepare_m5gfx.py` crée aussi une copie isolée de M5GFX 0.2.30 sous `dependencies/M5GFX` dans le dossier de travail. Il vérifie l'empreinte SHA-256 de `src/lgfx/v1/platforms/esp32/common.inl` avant deux corrections C++ : le tableau de pointeurs de registres GPIO devient `const` au lieu de `constexpr`, et l'initialisation du numéro de broche utilise une conversion explicite vers le type du champ. L'installation d'origine reste intacte. Les empreintes originale et corrigée sont consignées dans `m5gfx-clang-patch.json` ; les erreurs de conversion ne sont pas désactivées globalement.

`prepare_audio.py` applique le même principe à ESP8266Audio : dans sa copie isolée, `AudioOutputPDM.cpp` convertit explicitement la fréquence PDM vers `uint32_t`, comme le fait déjà la méthode de changement de fréquence de cette bibliothèque. Le fichier original doit correspondre à l'empreinte connue ; la preuve est écrite dans `audio-clang-patch.json`.

Tab5OS fournit explicitement ses déclarations de fonctions. `ctags_declared.py` vérifie l'unique fichier `.ino` avec le vrai compilateur cible avant de laisser Arduino poursuivre, ce qui remplace la génération facultative de prototypes par Ctags. L'intégralité du code est ensuite compilée normalement. Les diagnostics de détection Arduino qui analysent des fichiers C MicroPython comme du C++ ne doivent pas être confondus avec le résultat de leur compilation C finale.

Une reconstruction réussie vérifie la compilation et l'assemblage. Les essais de réactivité et de périphériques restent à effectuer sur le Tab5.

La 0.8.41 transmet aussi `-Wl,--wrap=esp_intr_alloc` au linker GNU pour le compteur matériel DSI. Cette voie Windows n’a pas été réexécutée pour cette livraison ; les preuves finales proviennent de Linux/GCC.

Depuis 0.8.48, cette voie recompile aussi les unités natives DSI et DW-GDMA cache-safe avec Clang et llvm-ar, dans le dossier isolé. Le même helper `tools/build_linux/prepare_dsi_cache_safe.py` contrôle les sources exactes. Le compilateur GCC ne s’exécute pas dans cette voie ; ses en-têtes et bibliothèques restent nécessaires. Les deux archives sont liées avant le SDK sous `--whole-archive`. Voir `COMPILATION.md` pour le périmètre et les validations requises.
