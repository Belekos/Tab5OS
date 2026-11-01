# Lanceurs et interface Python — 0.8.17

`launch_ui_0817.cpp` exécute les vrais `Tab5LaunchProfile.cpp` et `Tab5PythonUI.cpp` sur l'hôte. Seul le stockage Preferences est simulé par `Preferences.h`. Le test couvre la normalisation explicite SD/USB, le refus des chemins ambigus, les racines et longueurs limites, l'identité distincte des supports, la persistance des trois modes de lancement, une erreur d'écriture, un enregistrement corrompu, les raccourcis Python associés à leur génération de page et les actions contextuelles du lecteur.

Compilation : C++17, inclure d'abord ce dossier puis le dossier du sketch, compiler ces trois fichiers avec prise en charge des threads. Le test appelle les fonctions de production ; il n'exécute aucun ELF ni aucune opération sur le Tab5.

Les journaux se trouvent dans `../verification/runtime-0817-native.log` et `../verification/runtime-0817-syntax.json`. Les six unités du runtime et des outils ont passé la vérification syntaxique avec les en-têtes réels ESP32-P4, Arduino et LVGL. Ce contrôle ne remplace pas la compilation finale ni les essais tactiles sur l'appareil.

Les profils utilisent `sd:/Dossier/fichier.ext` ou `usb:/Dossier/fichier.ext`. Les anciens chemins logiques restent acceptés. Ce sont des chemins, sans expansion de commande. Le mode « Sans fichier » transmet un seul argument au programme (`argv[0]`) ; le mode avec fichier transmet aussi son chemin logique. Les extensions `.elf` et `.app.elf` suivent la même validation ABI et ELF avant exécution.

Les boutons Python peuvent déclarer `ui.shortcut(widget, 'back')`, `previous`, `next`, `enter`, `delete`, `up` ou `down`. Les touches de saisie restent réservées aux champs de texte. Les événements d'une ancienne page ne sont pas transmis à la suivante.
