# Relief et modèle Dessin — 0.8.51

Le moteur de relief a été modifié dans `image_selection.h`, avec des options
ajoutées à la fin de `IEFillOptions` dans `image_core.h`. Le reste de
`image_core.c`, notamment la propriété des sélections flottantes et leur
historique, est inchangé par rapport à .50.

## Géométrie et rendu

La distance chamfer 3/4 sur un octet, plafonnée à 64 pixels de largeur, est
remplacée par la transformée euclidienne du support raster. Une première
passe horizontale conserve les distances entières dans un champ `uint16_t`.
Des enveloppes paraboliques entières calculent ensuite les distances carrées
verticales exactes ; elles sont converties en distances Q4 (1/16 pixel).
Les pixels nuls du masque, les trous et l'extérieur du canevas sont inclus.
Le terme « exact » concerne les centres des échantillons raster, pas un
contour vectoriel continu.

Le champ est limité au rectangle englobant du remplissage. Il utilise
`2 × largeur × hauteur` octets, plus `10 × hauteur + 20` octets de travail
pour une colonne. Ce tampon de colonne est libéré avant le rendu. Le champ
de distance est réutilisé en place comme champ de hauteur Q6 (1/64 pixel).
Aucune deuxième image ni cache permanent n'est ajouté.

Les profils sont :

- Linéaire : hauteur proportionnelle à la distance, profil conique sur un disque.
- Circulaire : hauteur proportionnelle à `sqrt(1 - (1 - t)^2)`, où `t` est la
  distance normalisée. Le réglage indépendant de hauteur permet d'étirer ce
  profil ; il ne garantit donc pas une sphère géométrique exacte.
- Extrusion : hauteur constante, avec un bord abrupt dans le champ de hauteur.

La progression est définie en pixels ou en pourcentage de la plus grande
distance au bord dans toute la région. À 100 %, aucun plateau de largeur arbitraire
n'est imposé aux profils linéaire/circulaire. Des valeurs inférieures créent
volontairement un plateau. La quantification peut rendre égaux quelques
échantillons voisins d'un sommet arrondi. Dans plusieurs composantes de
largeurs différentes, la normalisation est globale : les branches étroites
ne sont pas artificiellement remontées à la hauteur maximale. Ce n'est pas
une reconstruction locale de l'axe médian.

La hauteur signée est comprise entre −256 et +256 pixels. L'azimut fait le
tour complet (0° vers la droite, 90° vers le bas dans l'image) et l'intensité
va de 0 à 200 %. L'élévation est fixe à 45°. Les normales utilisent toujours
25 lectures ; l'espacement des échantillons passe de 1 à 3 pixels pour les
profils larges afin d'atténuer les bandes dues au contour raster. Il reste
à 1 pour l'extrusion. Ce filtrage concerne l'éclairage, pas le masque ni
l'alpha. De petites facettes dues à la discrétisation peuvent subsister.

Le relief modifie les composantes RVB existantes. Il conserve exactement
l'alpha du calque, y compris celui des bords partiellement transparents.
Les pixels totalement transparents restent entièrement inchangés, RVB
caché compris. La couleur de premier plan n'ajoute aucune matière.
Hauteur ou intensité nulle conserve les pixels et n'ajoute pas d'historique.
Le résultat reste une image 2D éclairée, sans maillage 3D.

## Mémoire, progression et annulation

Les allocations du relief ont lieu après la génération du masque, une fois
la file de parcours et les buffers de progressivité libérés, et avant le
premier changement de pixels. Le budget du document reste fixé à 18 Mio.
Un refus libère les nouveaux buffers et conserve image, sélection et
sélection flottante. La politique préexistante peut évincer d'anciens états
d'historique lors de la recherche de mémoire ; elle n'est pas modifiée.

Chaque phase avance par lignes, colonnes ou groupes de nœuds. Le moteur
n'appelle jamais LVGL. L'annulation et l'historique existants restaurent le
raster et les métadonnées des sélections flottantes. Les données de travail
sont libérées à la fin, au refus ou à l'annulation.

## Vérifications exécutées

`tools/relief-tests-0851/run.py` compile le vrai moteur C avec ASan et UBSan.
Le résultat final compte 199 718 assertions, principalement des comparaisons
de pixels, et non autant de cas distincts. Les scénarios couvrent :

- Oracle euclidien indépendant par recherche exhaustive, masques aléatoires,
  trous, concavités et composantes de largeurs différentes.
- Profils complets, plateau volontaire, équivalence pixels/pourcentage,
  hauteurs signées, éclairage 359°/0°/360°, intensité et hauteur nulles.
- Conservation exacte de l'alpha et du RVB caché, toutes phases d'annulation,
  annuler/rétablir et interactions avec les sélections déplacées.
- Images de 1 pixel et dimensions 8192 × 1, 8192 × 64 et leurs transposées.
- 48 scénarios de refus d'allocation ponctuel ou persistant.
- Refus propre d'un relief 2000 × 1000 avec réserve d'affichage de 2 Mio.

Allocations suivies sur ordinateur, sans prétendre mesurer la PSRAM de
l'appareil :

| Cas | Pic suivi |
| --- | ---: |
| Relief réussi 1024 × 768 | 8 432 952 octets |
| Relief réussi 1100 × 610, réserve d'affichage 2 Mio comprise | 10 813 296 octets |
| Refus propre 2000 × 1000, réserve 2 Mio comprise | 16 107 884 octets |

Après destruction, chaque scénario vérifie zéro octet encore suivi.
LeakSanitizer est désactivé dans cet environnement ; ASan/UBSan et le registre
d'allocations restent actifs. Ces chiffres n'incluent pas tout LVGL ni les
autres services de l'OS.

La suite de sélection .50 a été copiée sans changement fonctionnel et
réexécutée sur .51 : 8 238 assertions, 420 déplacements, 216 refus simulés ;
la suite historique ajoute 34 840 assertions, 400 cycles et 336 refus.
Le témoin .49 reproduit les deux erreurs de déplacement et les sources .51
restaurent bien l'arrière-plan et les pixels fractionnaires. Le grand test
de sélection atteint 18 202 120 octets, réserve d'affichage de 2 Mio comprise,
sous le plafond de 18 Mio.

La compilation des unités C réelles avec le GCC P4/SDK restauré est attestée
par `relief-core-target-0851.json`. Cette vérification d'unités ne remplace
pas la compilation complète du firmware, menée séparément. Aucun essai sur
une Tab5 physique n'a été réalisé ici. Les temps de traitement et la
réactivité tactile pendant les grands reliefs restent à mesurer sur matériel.

La planche `tools/verification/relief-0851-preview.png` provient du vrai
moteur : linéaire, circulaire et extrusion, en saillie puis en creux.
