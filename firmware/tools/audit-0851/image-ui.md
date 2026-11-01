# Dessin : commandes et paramètres du relief — 0.8.51

## Causes vérifiées

Le menu de commandes utilisait son titre comme option zéro et remettait cette
option en sélection après chaque action. Ce faux élément restait surligné.
De plus, la table Édition déclarait six entrées pour neuf commandes réellement
définies : Couper, Coller et Supprimer n'étaient donc pas proposés.

Le code de LVGL 9.6 confirme que la validation d'une ligne au pointeur émet
VALUE_CHANGED même lorsque la valeur n'a pas changé, tandis que la validation
clavier via le bouton ne l'émet que si l'option diffère. Retirer seulement la
ligne de titre aurait rendu la première commande et les répétitions au clavier
inopérantes. Désactiver seulement selected_highlight aurait aussi caché le
repère de navigation au clavier.

## Corrections

Le texte du bouton reste indépendant des options. Toutes les lignes sont des
commandes ; leur nombre est déduit du tableau C++. Affichage ne contient plus
le doublon Options de l'outil ; la roue crantée reste disponible.

Les interactions de commande restent locales à Dessin. Les menus de paramètres
conservent leur sélection native. À l'ouverture au pointeur, aucune ancienne
commande n'est surlignée ; un mouvement de souris choisit la ligne effectivement
survolée, avec la couleur de sélection de l'OS, et quitter la liste efface ce
repère. Le calcul de ligne inclut le demi-interligne comme le hit-test natif de
LVGL. Le tactile conserve l'appui natif. Au clavier, flèches, Début et Fin
pilotent la ligne visible ; Entrée exécute explicitement la commande et permet
sa répétition. Échap ferme la liste. Aucun timer ni tampon d'image n'est ajouté.

Le relief utilise des champs indépendants des anciens paramètres de motif et
de direction du dégradé. La progression a deux valeurs conservées séparément,
en pixels et en pourcentage. L'extrusion masque la progression qui n'a pas
d'effet sur son profil constant. La hauteur signée, l'azimut de 0 à 359 degrés
et l'intensité disposent d'un curseur et de boutons tactiles −/+ par unité.
Les choix sont traduits. Les couleurs de premier et d'arrière-plan sont masquées
pour le relief, qui ombre les pixels existants en conservant leur alpha.

## Vérification

`tools/image-native-tests-0851/run.py` compile le code de production, le cœur
Dessin et LVGL 9.6 avec l'adaptateur TLSF de la version stabilisée. ASan et UBSan
restent actifs. Les scénarios précédents sont conservés et réexécutés, notamment
édition de texte, annulation, déplacement répété, imports réels, échecs de
mémoire et d'écriture, fermetures et suppression du parent.

Les scénarios ajoutés utilisent le pointeur LVGL réel et la route clavier
application puis Tab5Focus : titre absent de la liste, neuf commandes Édition,
règle de surbrillance, pixels de couleur du thème, répétition de la première
commande au clavier et au tactile, répétition d'une autre commande, passage du
survol au clavier, interligne non nul, réglages précis au tactile, valeurs de
progression indépendantes, extrusion et traitement progressif suivi d'Annuler.

Les empreintes, résultats et captures du dernier passage se trouvent dans
`tools/verification/image-native-0851.json`. Les dépendances matérielles sont
simulées aux frontières documentées ; ces essais ne mesurent pas la latence,
le DMA, les accès PSRAM/SD ni l'affichage sur une Tab5 physique.

Dernier passage complet : 9 632 assertions, 47 captures, cinq fermetures et
75 appels au clavier virtuel dans l’ensemble des scénarios. Le maximum des
allocations PSRAM suivies par ce banc reste de 12 437 385 octets, avec zéro
octet restant après fermeture. Il ne s’agit pas de la mémoire totale de l’OS.
Les empreintes des sources, du banc et des preuves ont été revérifiées après
ce passage. Le conteneur écran du banc est non défilable, comme le root réel
de l’OS. Aucun essai matériel n’a été exécuté.
