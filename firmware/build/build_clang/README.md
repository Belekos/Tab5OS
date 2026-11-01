# Windows / Espressif Clang

Cette voie historique reste expérimentale pour Tab5OS 0.8.53. Elle n'a pas été réexécutée et ne reproduit pas encore tous les réglages de cache et de détection de l'allocateur des parcours GCC. La [voie Linux/GCC](../../../docs/BUILD.md#linux-x86_64) est la référence.

Installer Arduino CLI, le core M5Stack 3.3.9, GCC esp-rv32/2601, Espressif Clang esp-20.1.1_20250829 et les bibliothèques aux versions du [guide](../../COMPILATION.md). Depuis `firmware`, adapter les chemins :

```powershell
python build/build_clang/compile_tab5_clang.py --release-root . --libraries-dir C:/Tab5Deps --work-dir C:/Tab5Build --arduino-data C:/Users/VOTRE_COMPTE/AppData/Local/Arduino15
```

`--arduino-cli`, `--clang-root` et `--gcc-root` sélectionnent les outils. `--clang-root` désigne le dossier contenant `bin/clang.exe`. Les en-têtes et bibliothèques GCC restent nécessaires. Le dossier de travail doit être hors du sketch.

`--mode prepare` prépare les arguments ; `--mode database` produit la base des commandes ; le mode `build` compile. Les scripts ne téléchargent pas Clang et ne téléversent pas le Tab5.

Les préparateurs vérifient les empreintes et modifient des copies isolées. `ctags_declared.py` vérifie les déclarations du sketch avec Clang. `normalize_clang_deps.py` harmonise les chemins de dépendances vérifiés. Les anciens vérificateurs liés à des chemins locaux de 0.8.28 ne sont plus conservés.
