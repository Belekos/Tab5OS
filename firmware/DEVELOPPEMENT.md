# Développer pour Tab5OS 0.8.35

## Choisir un format

| Besoin | Format |
| --- | --- |
| Interface de formulaires, scripts, traitement de petits fichiers | MicroPython et `tab5ui` |
| Calcul soutenu ou rendu de pixels avec budget mémoire maîtrisé | C compilé en ELF avec `Tab5AppApi.h` |
| Nouveau pilote ou service système | Modification du firmware et compilation complète |

Le SDK n'est pas le SDK Linux ou Arduino au complet. Un ELF ne peut importer que les points d'entrée `tab5_app_api_v1` à `tab5_app_api_v6`. Les fichiers `.bin` sont des firmwares, pas des applications.

## Un premier projet Python

Copier `examples/micropython/compteur` sur la carte dans `/Apps/compteur`. Le fichier `main.py` est le point d'entrée. Ouvrir **Applications Python**, puis actualiser avec **F5** si nécessaire. Le même script peut être ouvert dans l'éditeur et exécuté : sa vue est présentée directement.

L'exemple emploie les widgets natifs, le thème actif et des clés déjà traduites. Il montre une boucle qui attend 20 ms, lit les événements, ferme proprement dans `finally`, et associe Retour/précédent/suivant aux actions. Les raccourcis d'édition des champs demeurent ceux de l'OS.

Le module `tab5ui` expose une surface logique de `1100 × 500` et au plus 24 widgets par page. Utiliser `ui.WIDTH` et `ui.HEIGHT` pour les placements, garder de l'espace pour les traductions longues et prévoir plusieurs vues si le formulaire dépasse ce cadre. Ne pas copier les coordonnées de l'écran physique, créer sa propre boucle LVGL, ni réinitialiser l'affichage.

Les libellés passent par `ui.tr("Clé française")`. Pour une nouvelle clé, compléter les packs de langue et vérifier les glyphes ; changer seulement la langue du commentaire Python ne traduit pas l'interface. Les boutons de fermeture, remise à zéro et suppression doivent être espacés.

Les chemins Python sont logiques et absolus : `/Documents/texte.txt` pour la microSD et `/USB/Documents/texte.txt` pour le disque USB. Les notations `sd:/` et `usb:/` appartiennent aux champs du lanceur ; ne pas les envoyer à `open()`.

Utiliser `with open(...)` pour les fichiers et `try/finally` pour les ressources. Lire par blocs ; ne pas charger un ebook, une carte ou une archive entière dans une chaîne. `tab5fs.ilistdir()` est un itérateur à fermer si on interrompt son parcours. Pour un téléchargement, `tab5fs.download(url, destination, limit=...)` utilise le service HTTPS natif ; donner une limite adaptée. Ce module n'autorise pas une deuxième pile réseau autonome.

Un script ne peut pas supposer que tous les modules de MicroPython pour ESP32 standard sont inclus. Les modules disponibles sont ceux du port intégré et de `sdcard/MicroPython/lib`. Voir [MICROPYTHON_BIBLIOTHEQUES.md](MICROPYTHON_BIBLIOTHEQUES.md) pour l'installation de bibliothèques.

## Un premier ELF

Ouvrir un terminal dans `Tab5OS_Arduino_v0.8.53`. Avec la chaîne GCC Espressif installée :

```text
python tools/native-apps/build.py tools/native-apps/examples/pattern.c --toolchain CHEMIN/esp-rv32/2601/bin -o pattern.app.elf
```

Alternative vérifiée pour les exemples indépendants d'une bibliothèque C :

```text
python tools/native-apps/build.py tools/native-apps/examples/pattern.c --compiler clang --toolchain CHEMIN/esp-clang/bin -o pattern.app.elf
```

Python 3 et les outils `clang`, `ld.lld`, `llvm-readelf`, `llvm-strip` d'Espressif sont nécessaires pour cette alternative. Elle compile des objets RISC-V, puis les lie explicitement en ELF partagé. `--math` reste réservé à GCC et ses bibliothèques ; la voie Clang n'invente pas une libc embarquée.

Copier l'ELF dans `/Apps` ou `/Applications` sur microSD ou USB. Dans le lanceur, choisir **Sans fichier** pour `pattern.app.elf`. Les deux autres modes demandent un fichier ou mémorisent un fichier fixe. `hash_file.c` est un exemple de lecture qui reçoit le document en `argv[1]` ; son empreinte FNV-1a n'est pas cryptographique.

Le compilateur de projets refuse les imports extérieurs à l'API et conserve le précédent fichier de sortie si le nouveau programme échoue à compiler ou à valider. Les binaires d'exemple construits dans les tests sont des ELF32 RISC-V de type `DYN`. Cela ne prouve pas leur rendu sur une tablette physique.

### Contrat d'exécution

L'entrée est `int tab5_app_main(int argc, char **argv)`. Vérifier `version` et `size` avant d'appeler une API. Les chemins transmis sont `/...` ou `/USB/...`. Ne conserver aucun pointeur de l'OS après le retour de l'application.

- Allouer avec `api->base.allocate()` et libérer avec `release()` ; vérifier chaque échec. Le programme `pattern.c` utilise 24 Kio pour son image.
- Lire `should_stop()` régulièrement et appeler `sleep_ms()` pour rendre la main. Les pixels et échantillons sont copiés avant le retour des callbacks de présentation.
- Ne pas créer de tâches, de callbacks persistants, d'instance LVGL, ni appeler directement des symboles Arduino/ESP-IDF. Une interface native particulière nécessite d'étendre le service du firmware et son contrat.
- Après une annulation, fermer les fichiers et libérer les ressources, puis retourner. Le bouton d'arrêt ne justifie pas la suppression brutale d'une tâche encore propriétaire de fichiers ou de mémoire.
- Les limites de tas sont des plafonds, pas une promesse de mémoire libre. ABI 1 : 1 Mio ; ABI 2/3 : 8 Mio ; ABI 4 : 20 Mio, selon l'admission et les ressources déjà occupées. Les limites par bloc et de fichiers sont dans `Tab5AppApi.h`.
- Les ABI actuelles ne fournissent pas un client réseau générique. Ne pas contourner cette limite par des imports privés. L'accès matériel ABI 3 passe par le service commun décrit dans `tools/native-apps/HARDWARE.md`.

L'API ELF ne traduit pas automatiquement les messages propres à l'application. Le champ `language` permet de sélectionner ses traductions ; les exemples graphiques sans texte s'appuient sur les commandes communes de la vue native. La boîte de dialogue de l'OS ne suffit pas à rendre une application tierce multilingue.

## Organisation du firmware

| Couche | Responsabilité |
| --- | --- |
| Sketch principal et vues `Tab5*` | Navigation, LVGL, thème, interaction |
| `Tab5Task`, `Tab5Memory`, `Tab5ResourceManager` | Tâches, budgets et état des ressources |
| `Tab5NetworkGate` et services réseau | Admission des connexions et opérations natives |
| Stockage commun et hôte USB | Accès aux volumes, partage des périphériques |
| MicroPython / chargeur ELF | Exécution coopérative des applications et nettoyage |

Ne pas appeler LVGL depuis un worker de réseau ou de stockage. Produire un résultat borné et laisser la tâche UI l'afficher. Lorsqu'un traitement se termine, les destructeurs C++ doivent s'exécuter avant `vTaskDelete()` ; cette fonction ne déroule pas la pile C++.

## Vérifier une modification

Recompiler avec le [guide de compilation](COMPILATION.md). Après une modification de ressources SD, régénérer les fichiers concernés avec les scripts documentés.

Pour un essai matériel, relever la version, les périphériques, l'application ouverte, la mémoire avant/après et la séquence exacte. Répéter les ouvertures/fermetures et annulations, puis un retrait de support à un point prévu par l'interface. Une compilation seule ne valide ni la stabilité matérielle ni les besoins SRAM maximaux.

## ABI ELF 6 — canevas et images

`tab5_app_api_v6` conserve le préfixe ABI 5. `canvas_begin` crée un canevas de 800 × 480 au maximum, sans commandes de jeu. `input_poll` copie les événements tactiles et clavier ; CANCEL demande d’annuler le geste courant après perte ou saturation, CLOSE demande à l’application de traiter sa fermeture. `write_begin`, `write_chunk` (4 096 octets au maximum), `write_finish` exposent une écriture progressive limitée à `/Images/<nom>.bmp` ou `.t5i`, 16 Mio maximum et un seul flux. La fermeture de l’application abandonne un flux non validé. Les définitions exactes sont dans `Tab5AppApi.h`. Le source `tools/native-apps/image-editor/app.c` conserve l’exemple ELF historique de la 0.8.32 ; il n’est plus le programme distribué. L’éditeur courant utilise `Tab5ImageEditor.cpp` dans le firmware, avec LVGL et le moteur C partagé. Ne pas reprendre la reconstruction bitmap complète de l’ancien exemple pour une interface native fluide.
