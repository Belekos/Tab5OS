# Dessin

Ouvrir **Dessin** depuis l’accueil, ou choisir **Dessin** dans **Ouvrir avec…** sur une image dans l’explorateur. Les projets T5I s’ouvrent directement dans l’éditeur. L’application utilise le thème, la police, le focus et la saisie de l’OS.

## Outils et couleurs

Le rail gauche contient les outils ; l’icône sélectionnée est mise en évidence. Le volet de réglages est fermé au départ ; son ouverture le superpose à l’image sans changer son zoom ni son cadrage. Les menus du haut regroupent les fichiers, l’historique, la vue, les réglages d’image et les calques. Les deux couleurs du bandeau indiquent premier plan et arrière-plan ; le bouton gauche utilise la première couleur et le bouton droit utilise la seconde. La permutation reste disponible dans Couleurs et avec X.

| Outil | Usage |
| --- | --- |
| Pinceau / gomme | Tracer ou effacer avec taille, opacité, dureté et diffusion. Taille initiale : 2 px. |
| Sélection | Rectangle, ellipse, lasso libre, baguette contiguë et plage de couleurs. |
| Remplissage | Couleur unie, dégradés et motifs, sur zone contiguë ou toute la sélection. |
| Déplacer | Déplacer les pixels de la sélection. |
| Rectangle / ellipse / ligne | Glisser avec aperçu de la forme, puis relâcher pour valider. Épaisseur initiale : 2 px. |
| Texte | Cliquer dans l’image puis écrire à cet endroit. Entrée valide ; Échap annule. |
| Pipette | Prélever une couleur dans l’image. |
| Main | Déplacer la vue sans modifier les pixels. |
| Déformation locale | Pousser, gonfler ou pincer localement au pinceau. |

L’opacité limite la contribution de chaque geste : repasser sans relâcher ne rend pas le trait progressivement opaque. Relâcher puis commencer un nouveau trait ajoute une nouvelle contribution. La dureté règle le bord ; la diffusion disperse les points. Les formes de pinceau sont rond, carré, barre horizontale, verticale et deux diagonales.

## Sélections et remplissages

Choisir **Sélection**, ouvrir ses options et sélectionner la méthode. Le lasso suit le geste de la souris ou du doigt ; au moins trois points sont nécessaires. La baguette travaille sur une zone contiguë, la plage de couleurs recherche les pixels similaires. La tolérance règle l’écart de couleur accepté. L’échantillonnage peut utiliser les calques visibles.

Les modes remplacer, ajouter, soustraire et intersection permettent de composer le masque. Les commandes d’inversion et d’adoucissement agissent sur la sélection actuelle. **Progressivité** va de −32 à +32 pixels du document : une valeur négative adoucit uniquement l’intérieur, zéro conserve un bord net et une valeur positive adoucit de part et d’autre du contour. Une forme plus étroite que la progressivité intérieure peut devenir très faible ou disparaître. Le remplissage de toute la sélection applique aussi ce réglage, en restant limité au masque actif. Après une désélection, le premier tracé crée la forme choisie même si Ajouter, Soustraire ou Intersection est encore sélectionné ; l’inversion reste une commande explicite. Le masque conserve des degrés d’opacité, pas seulement une frontière tout-ou-rien ; les outils concernés respectent cette sélection.

Avec **Remplissage**, choisir la zone et le type : couleur unie, dégradé linéaire ou radial, hachures, damier ou points. Densité, direction et taille de motif adaptent le résultat. **Arrêter aux contours** et le seuil associé limitent la propagation sur les bords détectés. Les calculs longs peuvent être annulés ; une allocation refusée laisse l’image intacte.

## Relief

Le mode **Relief** de Remplissage ombre les couleurs déjà présentes sur le calque, en conservant leur transparence ; il ne peint pas avec la couleur de premier plan. Il simule une surface éclairée et ne produit pas de maillage 3D.

- **Conique** : profil linéaire, pente constante vers l’intérieur du contour.
- **Sphérique** : profil circulaire arrondi, dont l’amplitude dépend de Hauteur.
- **Extrusion** : hauteur constante dans la zone, avec changement de hauteur au bord.

**Progression** fixe la largeur du profil, en pixels du document ou en proportion de la distance maximale au contour. À 100 %, le profil atteint les points les plus éloignés du bord, sans plateau imposé ; une valeur plus faible laisse volontairement une zone plane. Les trous participent au contour. La proportion utilise le maximum de la zone entière : elle n’égalise pas la hauteur de toutes les branches étroites d’une forme. Extrusion n’utilise pas ce réglage.

**Hauteur** positive crée une saillie et une valeur négative un creux. **Azimut** règle la direction de la lumière de 0 à 359° : 0° à droite, 90° en bas, 180° à gauche, 270° en haut. **Intensité** règle le contraste de l’éclairage ; zéro conserve les couleurs d’origine. Ces réglages restent des paramètres de simulation sur une image 2D.

La roue crantée ouvre les options de l’outil ; cette commande n’est plus dupliquée dans Affichage. Les menus de commandes ne répètent pas leur titre dans la liste et ne conservent pas une ligne fictive sélectionnée. Le menu Aide contient également la page **Dessin : sélection et relief**.

Dans **Édition**, copier, couper, coller et supprimer agissent sur la sélection. Les raccourcis Ctrl+C/X/V et Suppr sont disponibles hors saisie de texte. Les flèches déplacent la sélection d’un pixel, Maj+flèches de dix pixels. Les déplacements successifs restaurent leur fond ; le presse-papiers reste disponible lors de Nouveau ou Ouvrir dans la même session de Dessin.

## Navigation et formes

La molette zoome autour du pointeur. Main ou bouton central déplace la vue sans ajouter d’état d’historique. Les commandes Ajuster et 100 % sont disponibles dans Affichage. Le clic droit utilise la seconde couleur pour les outils de dessin, sans ouvrir de menu contextuel. Échap annule le geste en cours ou ferme le dialogue actif.

L’ellipse se définit depuis deux coins de son cadre ou depuis son centre. L’option cercle impose des axes égaux. Les coordonnées apparaissent dès que le pointeur survole l’image. Pendant un geste, le bas de l’écran affiche aussi dimensions, longueur et angle selon l’outil. Le pourcentage de zoom reste lisible jusqu’à 1 600 %.

La déformation locale respecte le calque actif et la sélection. **Image → Transformation** applique rotation, échelle ou cisaillement à la sélection du calque actif, ou à l’image entière sans sélection. Annuler et rétablir fonctionnent sur les modifications du document ; les déplacements de vue ne créent pas d’état d’historique.

## Fichiers et limites

| Format | Ouverture | Enregistrement |
| --- | --- | --- |
| T5I | Projet avec calques et transparence. | Format de travail, conserve calques et transparence. |
| BMP | BMP non compressé 24/32 bits. | Image aplatie sur blanc. |
| JPG/JPEG | JPEG standard ou gris, dimensions originales. | Composite aplati sur blanc, qualité 92. |
| PNG | Image avec transparence, dimensions originales. | Enregistrer en T5I pour conserver la transparence. |
| WebP | Image avec transparence ; première image d’un fichier animé. | Enregistrer en T5I, BMP ou JPEG. |

Les imports conservent les dimensions originales : **aucune réduction automatique**, y compris pour JPEG et BMP. Si le format, les dimensions ou la mémoire ne permettent pas l’ouverture, un message explique le refus et le document déjà ouvert reste intact. Le zoom d’affichage ne change pas la résolution du document. Les fichiers JPEG et PNG sont limités à 16 Mio, et WebP à 2 Mio. JPEG progressif et orientation EXIF automatique ne sont pas gérés.

Le moteur accepte jusqu’à 8 192 pixels sur un côté, sous son budget de 8 Mio pour les pixels de tous les calques : la limite de côté n’autorise donc pas un carré de 8 192 pixels. Huit calques au maximum ; historique borné à seize états et 8 Mio selon mémoire. Le document, l’historique, la vue et les zones de travail partagent l’enveloppe native de 18 Mio. Une importation, un calque ou un geste peut être refusé si la mémoire nécessaire manque ; l’image n’est pas réduite pour contourner ce refus.

Enregistrer explicitement, de préférence en T5I pour continuer le travail. L’import conserve le fichier source. Changer d’application conserve le document et l’historique ; fermer une image modifiée demande confirmation. Les contrôles et le texte inséré dans les pixels utilisent la police de l’OS. La taille du texte se règle de 25 à 400 % ; le rendu est préparé avant validation, et un manque de mémoire conserve le texte et l’image.

La saisie de texte se fait à la position choisie dans l’image et constitue une seule opération dans l’historique après validation. Pendant la saisie, les lettres servent à écrire et ne changent pas d’outil.

Ctrl+Z annule, Ctrl+Y rétablit, Ctrl+S enregistre, Ctrl+O ouvre et Ctrl+N crée un document. Entrée et Échap fonctionnent dans les dialogues ; le clavier peut piloter les contrôles natifs.

Raccourcis d’outils : B pinceau, E gomme, S sélection, V déplacement de pixels, T texte, I pipette, H ou Espace Main, L ligne, R rectangle, O ellipse, W déformation. X échange les couleurs. Ctrl+plus/moins zoome ; Ctrl+0 ajuste et Ctrl+1 affiche à 100 %.

## Dossier par défaut
Les nouveaux projets et exports utilisent `/Picture`. Les projets personnels déjà présents sous `/Images` restent lisibles et peuvent être enregistrés à leur emplacement ; aucune migration ni aucun renommage automatique ne sont effectués.
