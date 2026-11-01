# Tests Explorateur 0.8.37

Exécuter `python tools/explorer-tests-0837/run.py --lvgl /chemin/lvgl-9.6.0 --lvgl-build /chemin/build-lvgl` depuis cette livraison. Le build LVGL doit fournir `liblvgl.a`, `lv_conf.h` et `host-lvgl.json`. Le compilateur C/C++ doit supporter AddressSanitizer et UndefinedBehaviorSanitizer.

Le script extrait les fonctions, constantes et structures du `.ino` et les branches de navigation SD/USB du menu de **cette livraison**. Les empreintes du rapport sont vérifiées avant et après chaque suite, pour refuser un résultat obtenu pendant une modification des sources.

`selection.cpp` utilise les événements réels LVGL 9.6, le focus, les styles et les modèles de sélection de production. Il vérifie notamment :

- ouverture initiale vide et absence de lecture du stockage ; menu Navigation explicite ;
- clic simple dans chaque volet : liste/aperçu et chemin complet immédiatement ;
- deuxième clic aux mêmes pixels après destruction de la ligne droite : focus du dossier gauche sans ouvrir son premier enfant ;
- double clic d'un fichier : aperçu conservé, focus sur son parent et commandes toujours ciblées sur le fichier ;
- aperçu texte réel, limite d'octets de production et coupure UTF-8 correcte ; routage de l'aperçu image ;
- clic de chevron, Ctrl/Maj, sélection multiple, appui long, contexte et clavier ;
- remontée depuis un aperçu, révélation asynchrone et pagination ;
- retrait/remontage d'un volume, fichier disparu et conservation des lignes pendant un scan ;
- Suppr sans sélection ne cible pas un document ouvert dans l'éditeur ; Annuler et Échap préservent l'aperçu pendant la confirmation de suppression ;
- aucun restylage de ligne après 200 mises à jour inchangées ; deux lignes restylées lors d'un déplacement simple de sélection ; aucun flush pendant 500 ms au repos.

Le stockage, la progression du scan et le décodage de l'aperçu image sont injectés. Les contrôles des boutons d'action sont remplacés par un compteur ; les critères des menus restent ceux de production. Ces mesures sont des budgets de travail UI, pas des temps sur Tab5. Les captures et le séparateur sont vérifiés séparément par `explorer-ui-tests-0837`.

`viewer.cpp` vérifie le véritable composite T5I et les branches du visualisateur (avec canevas et admission mémoire injectés). `context_menu.cpp` exerce les vrais menus LVGL, le texte et le focus, avec 512 cycles de création/destruction. Les journaux et le rapport sont écrits dans `tools/verification/explorer-0837*`.
