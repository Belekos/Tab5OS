# Revalidation du chemin d'affichage 0.8.44

```sh
python3 tools/display-media-tests-0844/run.py
```

Réutilise le scénario `tools/display-tests-0843/pipeline.cpp`, les modules de production actuels, LVGL 9.6 instrumenté et les doubles de frontières SDK/DMA/cache existants. Vérifie aussi que tous les modules `Tab5Display*` sont inchangés par rapport au ZIP 0.8.43 livré, et conserve les empreintes des dépendances M5GFX 0.2.30 réellement relues. Les chemins du SDK, du ZIP de référence et du LVGL peuvent être passés en arguments.

Le code de présentation/cache est réel ; le pilote IDF, le bus mémoire et la dalle sont modélisés. Le test ne démontre pas la disparition des flashs physiques. ASan/UBSan actifs, LeakSanitizer désactivé dans cet exécuteur. Aucun binaire hôte n'est livré.
