# Vérification visuelle de l’explorateur 0.8.38

Le harnais compile les fonctions de création des boutons et de la barre de menus extraites du `.ino`, les fichiers `Tab5ExplorerUi.inc` et `Tab5ExplorerRowStyle.inc`, ainsi que le vrai gestionnaire `Tab5Focus`. Il utilise LVGL 9.6.0 et la police `sdcard/System/font.ttf` livrée avec l’OS, aux tailles 22 et 24, dans une fenêtre de 1280 × 720 pixels.

```sh
python tools/explorer-ui-tests-0838/run.py \
  --lvgl /chemin/vers/lvgl-9.6.0 \
  --lvgl-build /chemin/vers/build-lvgl \
  --build /chemin/vers/build-ui
```

Le build LVGL doit fournir `liblvgl.a` et `lv_conf.h`, avec TinyTTF et le rendu RGB888 activés. Le programme de test est compilé avec AddressSanitizer et UndefinedBehaviorSanitizer. Les empreintes des sources sont contrôlées avant et après l’exécution.

Les assertions vérifient la construction des widgets sans faux contenu, le séparateur de 1 pixel, l’arbre dès le haut du panneau, les libellés sur une seule ligne, les quatre menus texte sans troncature et les deux contrôles compacts de tri/recherche. Les fixtures de dossiers distinguent les chevrons visibles des dossiers contenant uniquement des fichiers. Le chemin, les métadonnées, les actions et les résultats restent séparés. Le test vérifie aussi le maintien d’une sélection unique lors des restaurations de focus et l’absence de rafraîchissement pendant 500 ms d’inactivité. Les résultats de recherche montrent le nom et le dossier parent sur deux lignes.

Les captures produites pour chaque taille de police montrent une racine SD ouverte avec USB dans l’arbre, un sous-dossier ouvert, une sélection simple dans la liste droite qui conserve le dossier affiché, des résultats de recherche et un dossier Git avec son raccourci contextuel. Les données, les sondes de sous-dossiers et la disponibilité des commandes sont injectées : ces captures vérifient la mise en page réelle, pas le premier chargement depuis le stockage ni le déclenchement des actions. Les gestes, les menus, la sélection et la navigation sont testés séparément dans `explorer-tests-0838`, les supports et sondes par `explorer-tree-tests-0838`.

Les résultats et les dix captures sont écrits dans `tools/verification/explorer-ui-0838.json` et `tools/verification/explorer-ui-0838/`. Ce test hôte ne mesure ni le FPS sur la tablette ni le transport des entrées physiques. L’enregistrement dynamique des largeurs du shell est remplacé par la largeur physique de l’espace de travail, 1216 × 672 pixels.
