# Moteur de Dessin

[`image_core.c`](image_core.c) et [`image_core.h`](image_core.h) fournissent le
moteur raster de Dessin. Le firmware compile ce moteur via
[`Tab5ImageCore.c`](../../../Tab5ImageCore.c) ; son interface intégrée se trouve
dans [`Tab5ImageEditor.cpp`](../../../Tab5ImageEditor.cpp).

Le [guide de Dessin](../../../../DESSIN.md) décrit les outils, les formats et
l'utilisation de l'application intégrée.

Les sources `app.c` et `runtime.c`, avec leur recette `build.py`, permettent
également de compiler un exemple ELF ABI 6 à partir du même moteur. Depuis le
dossier du sketch :

```bash
python3 tools/native-apps/image-editor/build.py --toolchain /chemin/esp-rv32/2601/bin -o sdcard/Applications/AtelierImage.app.elf
python3 tools/native-apps/image-editor/run_tests.py
```

L'exemple possède sa propre interface ; les fonctions de l'application intégrée
sont documentées dans le guide de Dessin.
