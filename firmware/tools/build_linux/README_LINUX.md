# Compilation Tab5OS 0.8.52 sous Linux x86_64

Cette voie utilise GCC Espressif 14.2.0, paquet `esp-rv32/2601`, et le core Arduino M5Stack 3.3.9. Elle conserve la cible ESP32-P4 `prev3`, l'ABI, les exceptions et la table de partitions du projet. Aucun téléversement ni modification de l'installation Arduino globale n'est effectué.

Prérequis : Python 3, curl et un environnement Linux x86_64 avec environ 10 Go libres. Installer Arduino CLI 1.5.1 dans un dossier dédié `tab5-build/arduino-cli`. Le fichier `dependency-downloads.json` indique les URL et les SHA-256 des dépendances téléchargées pour cette reconstruction. Les scripts se placent à côté du dossier `tab5-build`, ou ailleurs.

```bash
mkdir -p tab5-build
curl -fL https://downloads.arduino.cc/arduino-cli/arduino-cli_1.5.1_Linux_64bit.tar.gz -o tab5-build/arduino-cli.tar.gz
tar --no-same-owner -xzf tab5-build/arduino-cli.tar.gz -C tab5-build arduino-cli
python3 prepare_linux.py --environment "$PWD/tab5-build"
python3 verify_downloads.py --environment "$PWD/tab5-build"
python3 compile_linux.py --release-root /chemin/Tab5OS_Ecriture_v0.8.52 --environment "$PWD/tab5-build" --sketch-name Tab5OS_Arduino_v0.8.52_Ecriture --jobs 4
python3 verify_linux.py --environment "$PWD/tab5-build"
```

`prepare_linux.py` installe les dépendances exactes dans ce dossier uniquement. La préparation télécharge les index et paquets officiels ; si un proxy HTTPS est défini dans l'environnement, il le transmet au réglage `network.proxy` d'Arduino CLI. La bibliothèque audio provient du tag 2.4.2, vérifié par SHA-256 ; sa métadonnée `library.properties` annonce 2.4.1.

La compilation utilise des copies isolées de LVGL (correctif d'allocation), de M5GFX (cinq corrections JPEG) , de M5Unified (ordre de démarrage audio optionnel Tab5) et de FS (backport errno limité à la source Arduino 3.3.9 vérifiée par empreinte). Le dernier correctif distingue les échecs d'allocation et conserve les causes d'E/S, sans modifier l'ordre des opérations ni le format des fichiers. Elle ne modifie pas le SDK ni le fichier source `Tab5CryptoLockCompat.c`. La commande, les versions et les empreintes des sources avant/après figurent dans `tab5-build/qa/`. Les images sont dans `tab5-build/build/`.

La vérification exige une compilation complète réussie, des sources inchangées, l'ELF RISC-V 32 bits à ABI simple précision, une image ESP32-P4 intègre, un hash ELF embarqué exact et une image tenant dans les partitions OTA. Elle contrôle aussi les symboles crypto ECDSA/SHA-AES, leur provenance dans l'objet de compatibilité du sketch, l'absence de l'ancien objet SDK correspondant et le chemin du décodeur JPEG effectivement compilé.

Une validation de compilation ne constitue pas un essai matériel. Wi-Fi/C6, affichage, extinction et périphériques restent à vérifier sur le Tab5.

La procédure ajoute automatiquement `-Wl,--wrap=esp_intr_alloc` pour le compteur matériel DSI. Le contrôle final vérifie aussi l’allocateur Bluetooth du sketch, l’absence du membre SDK remplacé et le placement en flash des tables SHA1/XZ. Voir `COMPILATION.md` pour une compilation manuelle ou Arduino IDE.

Les outils actifs `compile_linux.py`, `main_target.py` et `target_compile.py` découvrent le sketch courant ; les scripts suffixés d’anciennes versions restent historiques. Pour une compilation ciblée : générer la base avec `compile_linux.py --database-only`, puis utiliser `target_compile.py --database .../target-build/compile_commands.json --build ... --report ... Source.cpp`. Cette validation d’objet ne remplace pas le lien final.

Un contrôle séparé peut retirer la suppression Arduino des avertissements sur les unités sélectionnées, après compilation : `python3 warnings_current.py --environment /chemin/tab5-build --database /chemin/tab5-build/target-build/compile_commands.json Tab5Audio.cpp Tab5Storage.cpp`. Le firmware garde ses options normales. Ce contrôle ne remplace pas un test fonctionnel ou matériel.

Le correctif audio M5Unified est appliqué par `prepare_m5unified_audio.py` à une copie isolée, avec trois empreintes amont contrôlées. `verify_m5unified_audio.py`, appelé par `verify_linux.py`, vérifie le contenu complet de la copie, les inclusions des objets et les fonctions présentes dans l’ELF final. Aucun résultat de bruit audio physique n’est déduit de cette vérification.

Depuis 0.8.48, la préparation recompile deux unités natives exactes ESP-IDF 5.5.4 (`esp_lcd_panel_dpi.c`, `dw_gdma.c`) avec les options natives d'interruption et d'objets internes. Ces deux petites archives isolées précèdent les archives SDK à l'édition de liens ; le SDK installé reste intact. Le vérificateur final doit contrôler les drapeaux IRAM des interruptions, les objets et littéraux internes, la provenance des membres liés et le hook sans E/S en interruption. Le simple placement du code en IRAM, vérifié en 0.8.47, était insuffisant. Le message d'underrun est désormais émis par la tâche UI à partir du compteur IRQ. Les sources amont, leurs empreintes et le patch sont fournis sous `dsi_cache_safe/` et `prepare_dsi_cache_safe.py`.
