# Moteur raster d’Atelier Image

`image_core.c` est le moteur C réellement compilé dans l’application ELF. Il ne dépend ni de LVGL, ni d’un écran, ni d’un système de fichiers : l’hôte fournit allocation/libération, transferts de fichiers et coopération avec l’ordonnanceur.

## Représentation et limites

Les pixels sont RGBA non prémultipliés (`0xAABBGGRR`). Chaque calque contient un tableau de pixels, un nom de 23 octets, une opacité de 0 à 255 et un état visible/masqué. L’image accepte jusqu’à 8 calques et 1024 pixels par dimension ; la somme des pixels des calques reste inférieure à 8 Mio, avec 64 Kio réservés aux métadonnées et à l’allocateur. Ainsi, 1024 × 1024 convient à un calque et 512 × 512 à sept calques. La limite effective dépend également de la mémoire disponible.

L’historique conserve au plus 16 états et 8 Mio. Les états les plus anciens sont libérés quand la limite ou une allocation l’exige. Il n’y a pas de nombre fixe garanti d’annulations : une grande image en conserve moins. Les transformations réutilisent le dernier état d’annulation comme source afin d’éviter une troisième copie de l’image.

Avant de modifier les pixels, le moteur doit pouvoir conserver l’état précédent. En cas d’échec, il retourne une erreur et conserve le document ; une partie de l’ancien historique peut avoir été libérée pour tenter l’allocation. Les imports construisent un état séparé et ne remplacent l’image qu’après validation et lecture complète. Le moteur ne peut pas garantir à lui seul une écriture atomique sur SD ; l’application utilise la transaction de fichier de l’OS.

## Transactions

- `ie_begin` crée une étape d’annulation pour les primitives de dessin exécutées ensuite.
- `ie_gesture_begin` ouvre un geste pouvant être annulé ; `ie_commit` valide et `ie_cancel` restaure son état précédent. Les états de rétablissement antérieurs sont conservés jusqu’à la validation, sauf éviction due au manque de mémoire.
- Les opérations de calques, réglages, géométrie et déplacement créent elles-mêmes leur étape d’annulation.
- Le callback défini par `ie_set_yield` ne doit qu’offrir du temps à l’ordonnanceur. Il ne doit pas modifier le document ni appeler l’interface. Une opération engagée termine son état cohérent avant le traitement de la prochaine commande d’annulation/fermeture.

## Outils et formats

Le pinceau rond ou carré possède diamètre, opacité, dureté/contraste du bord et diffusion par points. La gomme modifie l’alpha. Le moteur propose ligne, rectangle/ellipse pleins et/ou contour, sélection rectangulaire, déplacement, mise à l’échelle indépendante des axes, cisaillement horizontal et rotations par quarts de tour. Le redimensionnement et les déformations utilisent le voisin le plus proche. Les réglages luminosité, contraste, saturation et gains rouge/vert/bleu s’appliquent à la sélection ou à l’image, sur le calque actif ou tous les calques visibles.

Le texte utilise une police bitmap originale de 5 × 7 pixels, agrandie par facteur entier. Les lettres latines, chiffres, ponctuation et accents français/cédille sont disponibles ; les caractères non couverts sont remplacés par `?`. Les calques sont des images raster : le texte dessiné n’est pas un objet typographique séparé.

- **BMP** : import non compressé 24 ou 32 bits `BI_RGB`, orientations haut-bas ou bas-haut ; le quatrième octet de `BI_RGB` est réservé et n’est pas interprété comme alpha. Export 24 bits, image aplatie sur fond blanc. Les BMP compressés ou à masques de couleur sont refusés.
- **T5I** : format de projet conservant les calques, leur ordre, leurs noms, l’alpha, l’opacité, la visibilité et le calque actif. L’historique et la sélection ne sont pas enregistrés. En-tête de 32 octets : magie `T5IMG001`, largeur, hauteur, nombre de calques, calque actif, indicateurs réservés à zéro et somme FNV-1a 32 bits. Les entiers sont en petit-boutiste. Chaque calque possède 28 octets de métadonnées (opacité, visibilité, deux octets réservés et nom de 24 octets), puis largeur × hauteur pixels RGBA. La somme couvre les 28 premiers octets de l’en-tête, les métadonnées et les pixels ; elle détecte les altérations ordinaires, sans servir d’authentification.

Les callbacks de lecture/écriture acceptent des transferts partiels, et chaque requête est limitée à 4096 octets.

## Vérification

Lancer `python3 run_tests.py`. Le programme compile le moteur de production avec AddressSanitizer et UndefinedBehaviorSanitizer, puis vérifie les outils, les calques, l’annulation/rétablissement, l’annulation d’un geste après une annulation, les codecs et entrées corrompues, 336 scénarios d’échec d’allocation et 400 cycles d’édition. Chaque scénario doit libérer toutes ses allocations. Le rapport lie les résultats aux SHA-256 des sources.

Ces tests sur ordinateur ne remplacent pas l’essai de l’écran, du tactile, de la SD et du chargeur ELF sur la tablette ESP32-P4.
