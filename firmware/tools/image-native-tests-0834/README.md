# Dessin 0.8.34 — interface native sur hôte

Ce banc exécute l’éditeur, le moteur image, les codecs JPEG, LVGL, la police OS et les services de focus/saisie de production. Seules les frontières matérielles, l’admission mémoire et le stockage SD sont adaptés au poste de test.

```sh
python3 tools/image-native-tests-0834/run.py --lvgl /chemin/lvgl --lvgl-build /chemin/lvgl-hote --m5gfx /chemin/M5GFX-corrige --build /chemin/banc-prive
```

Le chemin M5GFX doit désigner la même copie isolée corrigée que le firmware. Le rapport lie les sources, le décodeur et les captures par SHA-256. Les compteurs d’allocation suivis doivent revenir à zéro. ASan et UBSan sont actifs ; LeakSanitizer est désactivé dans cet environnement. Aucun débit d’affichage sur tablette n’est mesuré.
