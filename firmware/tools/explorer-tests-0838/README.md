# Tests Explorateur 0.8.38

Exécuter `python tools/explorer-tests-0838/run.py --lvgl /chemin/lvgl-9.6.0 --lvgl-build /chemin/build-lvgl` depuis cette livraison. Le build LVGL doit fournir `liblvgl.a`, `lv_conf.h` et `host-lvgl.json`. Le compilateur C/C++ doit supporter AddressSanitizer et UndefinedBehaviorSanitizer.

Le script extrait les fonctions, constantes et structures du `.ino` ainsi que les critères des menus de **cette livraison**. Les empreintes sont comparées avant et après la suite pour refuser un résultat obtenu pendant une modification des sources.

`selection.cpp` emploie le vrai gestionnaire d’entrée LVGL 9.6, les widgets, le focus, les styles et le modèle de sélection de production. Les parcours couvrent :

- premier accès sur la racine SD et réentrée dans une session déjà initialisée ;
- conservation du contenu, de la sélection, de la page et du focus pendant les sorties de l’Explorateur ;
- restauration du défilement après Git, puis après libération des widgets et modification externe du système de fichiers ;
- clic dossier à gauche : contenu et chemin actualisés à droite ;
- clic simple à droite : sélection seule, avec conservation des widgets, du contenu et du chemin ;
- double clic natif à droite : ouverture unique du dossier ou lancement du fichier, sans détection parallèle du geste ;
- activation clavier, chevrons, Ctrl/Maj, sélection multiple, appui long et menu contextuel ;
- Git comme sous-menu contextuel unique d’un dossier, avec actions différentes pour dépôt et dossier ordinaire ; visibilité de l’icône limitée au dossier configuré Git ;
- Renommer/Supprimer dans Édition, protections des racines de supports y compris conservation du presse-papiers lors d’une copie/coupe interdite ;
- recherche de dossiers et fichiers à droite, activation des résultats, pagination, conservation d’une recherche terminée et reprise d’une recherche interrompue ;
- révélation asynchrone et pagination de l’arbre ;
- conservation des cibles des commandes pendant les changements de focus ;
- Suppr sans sélection et annulation du véritable dialogue de suppression ;
- deux lignes restylées lors d’un déplacement simple de sélection, aucune après 200 mises à jour inchangées, aucun flush pendant 500 ms au repos.

Le stockage, la progression de l’arbre, le parcours de recherche et le décodage de l’aperçu image sont injectés. Les fonctions de navigation, de sélection, d’affichage de la recherche et de mémorisation de session sont celles de production ; les lancements d’application aboutissent à un enregistreur de chemin. Le scénario de sortie emploie le bloc réel de `showPage` qui interrompt les scans, sans exécuter le changement complet d’application. Le modèle réel d’arbre, ses sondes bornées de sous-dossiers et les transitions des supports sont testés séparément par `explorer-tree-tests-0838`. Les captures et les dimensions de l’interface sont vérifiées par `explorer-ui-tests-0838`.

Les budgets de travail UI mesurés ici ne sont pas des chronométrages du Tab5, ni une preuve d’absence de scintillement sur son écran physique. Les champs `limitations` du rapport précisent les frontières simulées.

`viewer.cpp` vérifie les branches de décodage et le composite T5I avec canevas et admission mémoire injectés. `context_menu.cpp` exerce les vrais menus LVGL, le texte et le focus. Les journaux et le rapport sont écrits dans `tools/verification/explorer-0838*`.
