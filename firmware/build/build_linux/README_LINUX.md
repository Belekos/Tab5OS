# Compilation Linux / GCC

Suivre la [procédure complète](../../../docs/BUILD.md#linux-x86_64) pour Tab5OS 0.8.53.

- `prepare_linux.py` prépare l'environnement Arduino isolé.
- `verify_downloads.py` vérifie les archives contre `dependency-downloads.json`.
- `compile_linux.py` sélectionne le sketch, prépare les adaptations et compile.
- `verify_linux.py` contrôle l'image et les adaptations liées. `--baseline` accepte une référence facultative ; les rapports historiques ne sont pas nécessaires.

Conserver ensemble les scripts `prepare_*` et `verify_*`, ainsi que le préparateur LVGL dans le dossier voisin `build_clang`.

Pour compiler un objet ciblé, générer la base avec `compile_linux.py --database-only`, puis utiliser `target_compile.py --database <compile_commands.json> --build <dossier> --report <rapport.json> <Source.cpp>`. `main_target.py --environment <dossier>` régénère et compile le sketch principal ; les résultats vont dans `qa` dans cet environnement. Ces contrôles d'objets ne remplacent pas le lien complet.
