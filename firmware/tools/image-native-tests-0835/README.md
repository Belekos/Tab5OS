# Dessin 0.8.35 — interface native sur hôte

Ce banc exécute l’éditeur, le moteur image, les codecs JPEG/PNG/WebP, LVGL, la police OS et les services de focus/saisie de production. Seules les frontières matérielles, l’admission mémoire et le stockage SD sont adaptés au poste de test.

```sh
python3 tools/image-native-tests-0835/run.py --lvgl /chemin/lvgl --lvgl-build /chemin/lvgl-hote --m5gfx /chemin/M5GFX-corrige --build /chemin/banc-prive
```

Les gestes utilisent le véritable moteur de hit-testing LVGL. Pour la souris, le banc reproduit l’ordre de production : événement primaire LVGL, puis callback des boutons logiques. Le clic secondaire est vérifié sur le pinceau, les formes et le texte ; il utilise la seconde couleur sans la permuter et conserve une seule étape d’annulation. Les pressions sur le panneau superposé et hors image ne peignent pas.

La saisie de texte s’exécute directement sur le canevas avec les vraies commandes clavier, y compris les lettres des raccourcis d’outils, les caractères Unicode à deux et quatre octets, Retour arrière, Entrée, Échap et la validation du clavier virtuel. Le banc vérifie les pixels, les historiques, le drapeau de modification, le redimensionnement à l’ouverture du clavier et la suppression externe du parent pendant une saisie.

Les autres vérifications couvrent le panneau fermé au démarrage, sa superposition sans modification du cadrage ni du tampon image, les coordonnées au survol, le pourcentage 1600 % avec la police OS à 24 px, les imports JPEG/BMP de 1300 × 300 sans réduction, l’alpha exact PNG/WebP, l’identification par signature malgré une extension trompeuse et la navigation jusqu’au bord d’une image de 8192 × 128. Les régressions de la 0.8.34 restent exercées : opacité, formes, calques, transformation, sauvegarde transactionnelle, annulation d’une écriture échouée et libération explicite.

Le chemin M5GFX doit désigner la même copie isolée corrigée que le firmware pour le décodeur JPEG. La copie hôte de miniz désactive uniquement son optimisation x86 par accès entiers non alignés afin d’exécuter le chemin portable déjà utilisé sur la cible RISC-V ; les deux empreintes sont consignées. Aucun diagnostic de sanitizer n’est désactivé. Le rapport lie les sources, la bibliothèque LVGL et les captures par SHA-256. Les allocations PSRAM suivies reviennent à zéro. ASan et UBSan sont actifs ; LeakSanitizer est désactivé dans cet environnement. Aucun débit d’affichage sur tablette n’est mesuré.
