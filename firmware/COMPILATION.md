# Reconstruire Tab5OS 0.8.53

Ce guide concerne la reconstruction des sources 0.8.53 sur ordinateur. Aucune commande ci-dessous ne téléverse un appareil.

<!-- build-status -->
**Compilation complète ESP32-P4 et vérification du BIN réussies.** Tests ciblés et limites dans [VERIFICATION.md](VERIFICATION.md). Aucun essai physique effectué.
<!-- /build-status -->

## Dépendances et cible

- Python 3 (`python3` sous Linux, `python` dans les exemples Windows), pour préparer notamment la copie isolée de LVGL.
- Arduino CLI 1.5.1 pour la voie Linux, core M5Stack 3.3.9, SDK ESP32-P4 et chaîne Espressif GCC 14.2.0, paquet RISC-V `esp-rv32/2601`.
- M5Unified 0.2.23, M5GFX 0.2.30, LVGL 9.6.0 et ArduinoJson 7.4.3.
- ESP8266Audio tag 2.4.2 : ce tag annonce encore `2.4.1` dans son fichier `library.properties`.
- Cible Tab5 : `ChipVariant=prev3`, PSRAM activée, flash DIO ; sélection Arduino `PartitionScheme=default` et table `partitions.csv` fournie dans le sketch, conservée.

Les voies fournies appliquent aussi un correctif ciblé à une **copie isolée de LVGL 9.6.0** : le moteur arabe contrôle ses deux allocations temporaires et conserve le texte UTF-8 original si la mémoire manque. `tools/build_clang/prepare_lvgl.py` vérifie l’empreinte du fichier source, prépare la copie et consigne la modification dans `lvgl-arabic-patch.json`. La bibliothèque LVGL installée reste intacte.

Le décodeur JPEG de M5GFX reçoit cinq corrections minimales : quatre expressions évitant le décalage de valeurs négatives, puis `memmove` pour le compactage d’un bord partiel dont les zones se chevauchent. `tools/build_linux/prepare_m5gfx_jpeg.py` vérifie le SHA amont et chaque substitution. Les voies Linux/GCC, Windows/GCC et Windows/Clang utilisent le même correctif ; la dépendance installée reste inchangée.

Le démarrage et la remise à zéro après arrêt audio utilisent également une **copie isolée de M5Unified 0.2.23**. `tools/build_linux/prepare_m5unified_audio.py` contrôle trois empreintes amont et prépare le chemin optionnel Tab5 : horloges et transfert silencieux I2S avant activation du codec/amplificateur, puis remise à zéro de l’interpolation après la fin effective du worker à l’arrêt. Les trois voies sélectionnent cette copie explicitement ; aucune installation globale ne change. La validation Linux contrôle les sources corrigées, leurs consommateurs et les méthodes réellement liées. Un essai audio sur appareil reste nécessaire.

Les dépendances doivent être disponibles avant la compilation. Les scripts laissent le SDK installé et ses scripts de placement mémoire intacts ; les adaptations nécessaires sont compilées dans un dossier isolé. Ils utilisent le fichier `lv_conf.h` fourni avec ce projet.

## Voie Linux/GCC pour la reprise 0.8.53

La procédure complète figure dans [tools/build_linux/README_LINUX.md](tools/build_linux/README_LINUX.md). Elle utilise Linux x86_64, Arduino CLI 1.5.1 et GCC Espressif 14.2.0. L’environnement isolé est restauré depuis les dépendances épinglées, avec des dossiers de compilation et de preuves distincts. Les rapports des anciennes versions restent historiques.

`prepare_linux.py` installe les versions prévues ; `verify_downloads.py` compare les 23 archives de dépendances et Arduino CLI au verrou SHA-256 ; `compile_linux.py` prépare les copies isolées de LVGL, M5GFX, M5Unified et FS puis lance Arduino CLI ; `verify_linux.py` contrôle le résultat de compilation, la stabilité des sources, l’architecture de l’ELF, l’intégrité de l’image, son hash ELF embarqué, les partitions OTA et la provenance du correctif crypto. Le répertoire de travail doit rester hors du sketch. Le SDK et ses scripts de placement mémoire ne sont pas modifiés.

Les URL et SHA-256 des dépendances de cette reconstruction figurent dans [dependency-downloads.json](tools/build_linux/dependency-downloads.json). Les archives téléchargées ont été vérifiées contre les empreintes attendues des versions retenues. Ces contrôles ne remplacent pas ceux du firmware.

### Corrections de typage GCC conservées de la 0.8.29

Cette chaîne définit `int32_t` comme `long int`, alors que `int` et `long` font tous deux 32 bits. Leur taille identique ne rend pas leurs pointeurs interchangeables.

- **GitView et SDR** : `std::max<int32_t>` explicite le type des valeurs de défilement et de hauteur renvoyées par LVGL.
- **PCRE2** : les variables locales `min` et `max` prennent le type `int` attendu par `read_number(int*)`. La borne 65 535 et la sentinelle 65 536 restent représentables.
- **libgit2** : les variables, pointeurs internes et paramètres de callback suivent les contrats `int32_t`/`uint32_t` existants ; les valeurs temporaires ne sont recopiées qu’après une lecture réussie. Les signatures publiques sont conservées.

Les patches [PCRE2_GCC_0829.patch](Tab5OS_Arduino_v0.8.53_Ecriture/src/tab5git/PCRE2_GCC_0829.patch) et [LIBGIT2_GCC_0829.patch](Tab5OS_Arduino_v0.8.53_Ecriture/src/tab5git/LIBGIT2_GCC_0829.patch) restent avec les sources et leurs métadonnées de provenance. [gcc-source-fixes.json](tools/verification/build-0829/gcc-source-fixes.json) consigne les changements et empreintes avant/après ; [gcc-integer-types.json](tools/verification/build-0829/gcc-integer-types.json) établit les types de la cible. Le contrôle historique 0.8.29 de syntaxe cible de 247 fichiers Git/SSH est consigné dans [vendor-syntax-gcc.json](tools/verification/build-0829/vendor-syntax-gcc.json). Ce contrôle n’est ni une édition de liens ni un essai fonctionnel Git/SSH.

## Alternative Arduino/GCC sous Windows

Cette voie est conservée mais n’a pas été réexécutée pour la 0.8.53.

Depuis le dossier décompressé :

```powershell
.\Compiler_Tab5OS.ps1 -InstallDependencies
```

Cette première commande télécharge les dépendances dans `build-environment` ; elle nécessite donc une connexion réseau. Les compilations suivantes utilisent :

```powershell
.\Compiler_Tab5OS.ps1
```

Le script détecte le sketch de cette livraison. `-CheckOnly` affiche sa version sans compiler ; `-SketchPath` sélectionne explicitement un dossier si plusieurs versions sont présentes.

Le paramètre `-ArduinoCli` accepte le chemin complet de `arduino-cli.exe`. Les bibliothèques globales de l'utilisateur ne sont pas modifiées.

## Alternative Espressif Clang sous Windows

Dans l’environnement Windows de préparation de la 0.8.28, GCC et le programme de génération de prototypes `ctags` échouaient au démarrage avec l’erreur DLL `WinError 623`. Cette version avait été compilée avec **Espressif Clang `esp-20.1.1_20250829` et GNU ld 2.43.1**, en conservant le SDK, l’ABI `ilp32f`, la cible RV32/P4 et les exceptions C++ du projet. Ce constat historique ne décrit pas l’environnement Linux utilisé pour reprendre la 0.8.49.

Les scripts Clang Windows restent fournis mais n’ont pas été réexécutés pour la 0.8.53. Les preuves Windows/Clang de la 0.8.28 gardent leur portée historique.

Conserver **tout le dossier `tools/build_clang`**. `compile_tab5_clang.py` utilise Arduino CLI avec cette chaîne ; `ctags_declared.py` vérifie les déclarations explicites du sketch avec Clang, sans dépendre du binaire Windows `ctags`. Les autres scripts préparent les copies de bibliothèques adaptées. Aucun cache d'une ancienne version du projet n'est nécessaire. Le guide `tools/build_clang/README.md` détaille les paramètres de cette voie.

Pour réutiliser les objets déjà compilés sous Windows, le script harmonise le premier chemin des fichiers de dépendances `.d` produits par Clang. Il vérifie l’identité du fichier source et la stabilité des fichiers avant chaque remplacement ; les sources, les objets et leurs dates restent intacts. Arduino CLI conserve ses contrôles habituels de dépendances. Le réemploi a été observé dans une compilation réelle : [preuve du cache](tools/verification/clang-cache-0818.json). Cette preuve ne remplace pas la validation finale du firmware.

La voie Clang applique des corrections C++ ciblées sur des **copies isolées** de M5GFX 0.2.30 et ESP8266Audio : qualification d'un tableau de pointeurs GPIO et conversion du numéro de broche dans `common.inl`, conversion explicite de la fréquence I2S dans `AudioOutputPDM.cpp`. Les empreintes SHA-256 des fichiers attendus sont contrôlées avant modification et les changements sont consignés. Les bibliothèques d'origine, le SDK et ses scripts mémoire restent intacts ; les erreurs de conversion ne sont pas désactivées globalement.

Préparer un dossier contenant directement les cinq bibliothèques : `M5Unified`, `M5GFX`, `lvgl`, `ArduinoJson` et `ESP8266Audio`. Adapter les chemins de l'exemple :

```powershell
python tools/build_clang/compile_tab5_clang.py --release-root . --libraries-dir C:/Tab5Deps --work-dir C:/Tab5Build --arduino-data C:/Users/VOTRE_COMPTE/AppData/Local/Arduino15
```

Le dossier de travail et son dossier de compilation doivent être **hors du dossier du sketch**. Les paramètres `--arduino-cli`, `--clang-root` et `--gcc-root` permettent d'indiquer des installations ailleurs que les emplacements par défaut. `--clang-root` désigne le dossier qui contient `bin/clang.exe`. Le paquet GCC reste nécessaire pour ses en-têtes et bibliothèques, même si son compilateur ne s'exécute pas.

Le script conserve dans le dossier de travail les arguments, versions des outils, journaux, code de sortie et empreintes des sources avant/après compilation. Il refuse de valider un build dont les sources ont changé pendant l'exécution. `--mode prepare` prépare les paramètres sans compiler ; `--mode database` produit uniquement la base des commandes de compilation. **Seul le mode `build`, suivi des contrôles d'image, peut produire un firmware livrable.**

Les scripts ne téléchargent pas Espressif Clang et ne l'installent pas automatiquement. Les tests hôtes décrits dans VERIFICATION.md sont distincts de cette compilation RISC-V.

Les sources de libgit2 et libssh2 sont intégrées au sketch avec leur configuration P4 ; aucune bibliothèque système Git ou SSH ni archive précompilée externe n’est nécessaire. Les adaptateurs utilisent la pile réseau, le stockage et mbedTLS du SDK existant. Les versions et adaptations figurent dans BIBLIOTHEQUES.md et les fichiers UPSTREAM des composants.

## Compteur matériel des défauts d’affichage

Les scripts ajoutent `-Wl,--wrap=esp_intr_alloc` pour observer l'état et les drapeaux de l'interruption DSI. Le défaut d'alimentation est compté par le hook fort `tab5_display_underrun_isr`, appelé depuis l'unité native DSI recompilée. Ce hook ne journalise rien ; la tâche UI consomme ensuite le compteur et produit le diagnostic sans charger l'interruption. Les compteurs, arguments et fonctions de ce chemin doivent rester en mémoire interne.

Une reconstruction 0.8.53 doit utiliser les scripts fournis, qui préparent et lient aussi les deux unités natives décrites ci-dessous. Ajouter uniquement l'option `--wrap` dans l'IDE ou Arduino CLI ne reproduit pas ce firmware corrigé. `build_opt.h` ne transmet pas cette option à l'édition de liens du core. Le contrôle final vérifie le code effectivement lié, les interruptions et leurs données, et les six définitions de l'allocateur Bluetooth révisé. Les allocations génériques de l'hôte NimBLE utilisent la PSRAM ; les allocations marquées internes/DMA gardent leurs contraintes.

La 0.8.49 prépare aussi une copie isolée de la bibliothèque FS Arduino 3.3.9 avec un backport errno limité et vérifié par SHA-256. Les commandes Linux/GCC, PowerShell et Clang Windows sélectionnent explicitement cette copie, comme pour LVGL et M5GFX ; le SDK installé reste intact. Seul le parcours Linux/GCC est exécuté pour cette livraison.

## Interruptions écran en 0.8.49

Les parcours Linux/GCC, PowerShell/GCC et Windows/Clang préparent les unités natives DSI et DW-GDMA isolées avec les configurations cache-safe d'ESP-IDF 5.5.4. Ne pas remplacer cette préparation par un simple `-D` lors du lien : cela ne change pas les archives précompilées. Ne pas retirer le groupe `--whole-archive ... --no-whole-archive` généré. Il conserve les noms de membres attendus par le script de liaison IRAM et remplace uniquement les deux unités concernées. Le SDK installé, le pilote MIPI et son timing restent ceux du projet. Les tests Windows restent non exécutés dans cette livraison Linux.

## Adaptateurs mémoire 0.8.49

Les scripts vérifient quatre empreintes SDK avant d’activer `TAB5_HEAP_PROBE_PINNED=1`. Un SDK inconnu conserve le chemin public ESP-IDF, sans refuser une fonction de l’OS. Le lien ajoute `--wrap=esp_cache_msync`. Le contrôle final examine les appels réellement liés, l’allocateur TLSF LVGL et le nouveau `Speaker_Class::stopAndWait`. Recompiler LVGL et M5Unified avec les copies et la configuration fournies ; ne pas réutiliser leurs objets d’une ancienne version.
