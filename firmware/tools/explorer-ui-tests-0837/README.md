# Vérification visuelle de l’explorateur 0.8.37

Le harnais compile les fonctions de création des boutons et de la barre de menus extraites du `.ino`, les fichiers `Tab5ExplorerUi.inc` et `Tab5ExplorerRowStyle.inc`, ainsi que le vrai gestionnaire `Tab5Focus`. Il utilise LVGL 9.6.0 et la police `sdcard/System/font.ttf` livrée avec l’OS, aux tailles 22 et 24, dans une fenêtre de 1280 × 720 pixels.

```sh
python tools/explorer-ui-tests-0837/run.py \
  --lvgl /chemin/vers/lvgl-9.6.0 \
  --lvgl-build /chemin/vers/build-lvgl \
  --build /chemin/vers/build-ui
```

Le build LVGL doit fournir `liblvgl.a` et `lv_conf.h`, avec TinyTTF et le rendu RGB888 activés. Le programme de test est compilé avec AddressSanitizer et UndefinedBehaviorSanitizer. Les empreintes des sources sont contrôlées avant et après l’exécution.

Les assertions vérifient l’absence de texte et de lignes implicites à la création, le séparateur de 1 pixel, les noms de fichiers sur une seule ligne, les menus sans troncature, la séparation entre le chemin, les métadonnées, les actions et l’aperçu, le maintien d’une sélection unique lors des restaurations de focus et l’absence de rafraîchissement pendant 500 ms d’inactivité. Une image de 420 × 280 pixels vérifie le placement du widget d’aperçu.

Les captures produites pour chaque taille de police montrent l’entrée vide, un dossier ouvert, une sélection dans la liste droite qui conserve le dossier affiché (cas Ctrl/Maj), un aperçu texte et un aperçu image. La disponibilité des commandes et les données de fichiers sont injectées : ces captures vérifient la mise en page réelle, pas le déclenchement des actions. Les gestes, les aperçus depuis le stockage et la navigation sont testés séparément dans `explorer-tests-0837`.

Les résultats et les dix captures sont écrits dans `tools/verification/explorer-ui-0837.json` et `tools/verification/explorer-ui-0837/`. Ce test hôte ne mesure ni le FPS sur la tablette ni le transport des entrées physiques. L’enregistrement dynamique des largeurs du shell est remplacé par la largeur physique de l’espace de travail, 1216 × 672 pixels.
