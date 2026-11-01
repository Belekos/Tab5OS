# Atelier Image — Tab5OS 0.8.32

Application ELF native ESP32-P4, ABI 6. Ouvrir `Applications/AtelierImage.app.elf`
depuis l'explorateur. Aucun argument n'est requis. Un chemin BMP ou T5I peut aussi
être transmis comme argument, notamment avec « Modifier image » dans l'explorateur.

## Outils disponibles

- Pinceau rond ou carré : diamètre, opacité, contraste du bord (doux/dur), diffusion.
- Gomme, pipette, lignes, rectangles et ellipses : plein, contour ou les deux.
- Sélection rectangulaire limitant les opérations, déplacement, étirement indépendant
  horizontal/vertical, inclinaison et rotation par quarts de tour. Les transformations
  utilisent le plus proche voisin ; les parties déplacées hors image sont coupées.
- Calques : ajout, duplication, suppression, nom, ordre, visibilité, opacité et fusion.
- Annuler/rétablir ; un trait complet correspond à une opération. Échap annule un
  geste en cours, y compris sa modification d'opacité, sans perdre le rétablissement
  antérieur sauf éviction d'historique imposée par la mémoire disponible.
- Texte bitmap 5×7, taille et couleur réglables, caractères ASCII et accents français
  courants. Les autres caractères sont remplacés par « ? » ; aucune police vectorielle.
- Luminosité, contraste, saturation et gains rouge/vert/bleu sur le calque actif ou
  tous les calques visibles. La sélection délimite aussi ces réglages.
- Dimensions en pixels, redimensionnement, recadrage de la sélection.

## Fichiers et limites

- **T5I** : projet natif conservant les pixels RGBA, calques, noms, ordre, visibilité
  et opacité. Intégrité du contenu vérifiée à l'ouverture. L'historique et la sélection
  ne font pas partie du fichier.
- **BMP** : import 24/32 bits non compressé BI_RGB. Le quatrième octet d'un BMP32
  BI_RGB est réservé et n'est pas traité comme de la transparence. Export RGB24
  aplati sur fond blanc ; les calques et la transparence sont conservés uniquement
  dans T5I. JPEG, PNG, GIF et BMP compressés ne sont pas pris en charge.
- Enregistrement dans `/Picture/<nom>.t5i` ou `/Picture/<nom>.bmp` ; l'application
  demande confirmation avant de remplacer un fichier existant. L'OS écrit un fichier
  temporaire puis valide le remplacement. Un échec conserve le fichier précédent.
- 1 à 1 024 pixels par côté, 8 calques au maximum ; pixels du document limités à
  8 Mio moins 64 Kio. Les allocations peuvent être refusées plus tôt si la mémoire
  réellement disponible est insuffisante, sans modifier l'image courante.
- Historique borné à 16 états et 8 Mio. Les plus anciens états sont libérés sous
  pression mémoire. L'allocateur demande des blocs PSRAM à la demande (minimum
  256 Kio, grands blocs alignés sur 4 Kio), maximum 16 Mio / 30 blocs, et rend
  immédiatement chaque bloc entièrement libre à l'OS pendant l'utilisation.
- Les cadres vidéo sont en dehors de ce budget : 800×480 RGB565 dans l'application,
  plus deux tampons copiés appartenant à l'OS. Les appels sont coopératifs ; l'OS
  garde la gestion de l'écran, de la mémoire, des entrées et des fichiers.
- Le travail reste en mémoire tant que l'utilisateur n'a pas enregistré. Quitter,
  Nouveau et Ouvrir demandent confirmation en présence de modifications. Une
  interruption forcée ou une coupure d'alimentation peut perdre le travail non enregistré.

## Commandes

Les boutons fonctionnent au toucher et à la souris. Le clavier physique saisit les
noms et le texte. Entrée valide, Échap annule ; un clavier tactile est également
présent. Ctrl+Z/Ctrl+Y annulent/rétablissent ; Ctrl+Maj+Z rétablit ; Ctrl+S enregistre
un projet, Ctrl+O ouvre, Ctrl+N crée. B : pinceau ; E : gomme ; S : sélection ;
V : déplacement ; T : texte. Les réglages de couleur sont appliqués explicitement
avec « Appliquer » et peuvent être annulés.

## Compilation et vérification

Depuis le dossier du sketch :

```
python3 tools/native-apps/image-editor/build.py --toolchain /chemin/esp-rv32/2601/bin -o sdcard/Applications/AtelierImage.app.elf
python3 tools/native-apps/image-editor/run_tests.py
```

Le compilateur Espressif GCC 14.2.0 produit un ELF32 RISC-V ET_DYN, entrée
`tab5_app_main`, seul import `tab5_app_api_v6`, pile PSRAM déclarée de 48 Kio.
`image_core.c` et `app.c` sont compilés directement dans les tests hôtes, avec
ASan et UBSan. Les tests de l'OS vérifient séparément le courtier ABI6 ; ils ne
remplacent pas une vérification sur tablette. Les captures hôtes proviennent des
fonctions de rendu réelles de l'application. Aucun test tactile physique n'a été effectué.
