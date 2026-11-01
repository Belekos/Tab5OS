# Cartes / focus 0.8.30 — régression hôte

Exécution :

```sh
python3 tools/maps-tests-0830/run.py --libraries /chemin/Arduino/libraries
```

Dépendances : GCC/G++, ar, Python 3 ; LVGL 9.6.0 et ArduinoJson 7.4.3. Le script ne télécharge rien, ne change aucune bibliothèque installée et crée son répertoire de compilation hors du projet. `--build` permet de conserver les objets entre exécutions.

Le harnais utilise les fonctions de résultat de recherche extraites exactement du sketch, le gestionnaire de focus, la saisie, le document, le vrai moteur LVGL/TinyTTF et la police fournie sur SD. La configuration LVGL reprend celle du firmware, avec les allocations système de l’hôte à la place de la PSRAM. Le correctif de mise en forme arabe déjà livré est appliqué à l’objet correspondant.

Les scénarios couvrent : noms composés, accents et encodage URL ; lecture JSON filtrée, données invalides et panne d’allocation ; sélection d’un résultat par Entrée et Espace ; validation d’un champ qui ferme sa fenêtre et en crée une nouvelle ; destruction pendant FOCUSED ; suppression d’une ligne en conservant la liste ; déplacement d’une ligne avant suppression de son ancien parent ; boutons, cases et interrupteurs normaux.

`Tab5Focus-0829.cpp.txt` est le source 0.8.29 conservé pour reproduire les accès à des objets libérés avec AddressSanitizer. Le script exécute les mêmes scénarios avec ce source puis le source corrigé, sous ASan et UBSan. Les échecs de référence sont attendus. Les journaux de ces échecs et le rapport `tools/verification/maps-0830.json` accompagnent la livraison.

L’espace était déjà transmis sous la forme `%20`. Les défauts reproduits concernent la durée de vie des contrôles de recherche. Cela ne prouve pas que le seul fait de saisir deux mots provoquait l’incident précis sur la tablette. Aucun essai matériel, réseau/TLS, service de géocodage réel ou confirmation de la panne physique n’est revendiqué. LeakSanitizer est désactivé car l’environnement hôte ne permet pas son inspection des processus ; ASan/UBSan restent actifs.
