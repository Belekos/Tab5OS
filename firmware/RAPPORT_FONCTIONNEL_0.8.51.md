# Tab5OS 0.8.51 — Menus et relief de Dessin

## Référence et périmètre

Cette version part de l’archive finale 0.8.50 livrée, SHA-256 `ea13e38382767ee6e061d5a099b553f6a42461454b1a5bbd7bc653893a802c3b`. L’archive et les sources de référence sont conservées. Les preuves anciennes restent historiques ; les résultats ci-dessous correspondent aux exécutions 0.8.51.

Le changement reste centré sur Dessin. Cinq fichiers de production non générés diffèrent de la 0.8.50 : `Tab5ImageEditor.cpp`, `image_core.h`, `image_selection.h`, `Tab5Help.cpp` et le numéro de version du sketch principal. Les traductions, ressources générées, tests et documents associés sont actualisés. Les pilotes, services audio/vidéo, mécanismes d’affichage/cache, allocateur LVGL, partitions et choix de sauvegarde sont conservés. Le contrôle spécifique porte sur 44 fichiers stabilisés de la 0.8.49, également présents dans la 0.8.50.

## Causes établies et corrections

| Module | Cause vérifiée dans le code | Correction |
| --- | --- | --- |
| Menus de Dessin | Le titre était inséré comme option zéro et resélectionné après chaque commande. | Titre séparé de la liste ; seules les commandes sont affichées. Le survol et les flèches mettent en évidence la ligne effectivement visée. |
| Menu Édition | Neuf commandes étaient définies mais le compteur en exposait six. | Taille déduite du tableau ; Couper, Coller et Supprimer sont accessibles. |
| Activation au clavier | LVGL peut ne pas émettre de changement de valeur lorsqu’Entrée valide la même option. | Activation explicite des menus de commandes, testée avec répétition. Les listes de réglages gardent leur comportement de sélection normal. |
| Menu Affichage | Options de l’outil doublonnait la roue crantée. | Commande retirée d’Affichage ; roue crantée conservée. |
| Relief | Champ de distance sur un octet, approximation 3/4, largeur plafonnée à 64 px et profil unique produisant un plateau. | Distance euclidienne au contour raster, profils Conique, Sphérique et Extrusion ; progression en pixels ou en proportion de la distance maximale. |
| Éclairage | Quatre directions fixes et application de la couleur de remplissage. | Azimut 0–359°, intensité 0–200 %, hauteur −256 à +256 px ; ombrage des couleurs existantes avec conservation exacte de l’alpha. |
| Traductions | Les nouveaux contrôles et leur aide devaient tenir dans les limites existantes. | Quatorze langues mises à jour ; suppression d’un seul ancien paragraphe Explorateur dont l’absence de référence a été vérifiée. Limite et lecteur de langue inchangés. |

Les boutons ± permettent des réglages d’un degré, d’un pixel ou d’un pourcentage sans souris ni clavier physique. Les profils utilisent des libellés courts pour laisser visible la flèche du menu. Les commandes répondent aux événements ; aucun minuteur de survol ou de rafraîchissement périodique n’est ajouté.

## Géométrie et limites explicites

**Conique** applique une hauteur linéaire avec la distance au contour. **Sphérique** utilise un profil circulaire ; l’ajustement indépendant de la hauteur peut l’étirer. Ces noms décrivent le profil : une sélection arbitraire ne devient pas une sphère ou un cône géométriquement exact. **Extrusion** donne une hauteur constante à la région, avec un bord éclairé ; son réglage de progression est masqué car sans effet sur ce profil.

Pour les profils progressifs, 100 % atteint la distance intérieure maximale de la région et évite le plateau imposé de la version précédente. Une distance plus courte peut volontairement créer un plateau. Sur des branches de largeurs différentes, la normalisation reste globale : elle ne donne pas la même hauteur à chaque branche et n’est pas une construction de l’axe médian local.

Le signe de la hauteur distingue saillie et creux. L’azimut suit les coordonnées de l’image : 0° à droite, 90° en bas, 180° à gauche, 270° en haut. L’élévation de la lumière reste fixée à 45°. La hauteur et l’intensité nulles ne modifient ni les pixels ni l’historique.

Le résultat reste un relief dans une image raster, sans maillage 3D ni déplacement de la silhouette. Les pixels transparents ne reçoivent pas de matière ; les couleurs déjà présentes sont ombrées. Les trous et le masque de sélection sont respectés. Une discrétisation résiduelle du contour demeure possible, surtout pour les petites formes. L’espacement adaptatif des normales réduit les stries visibles sans buffer supplémentaire et conserve 25 échantillons par pixel traité.

## Ressources et intégrité

Le champ de travail utilise deux octets par pixel du rectangle utile, plus une colonne de travail proportionnelle à sa hauteur. Les buffers de propagation et de progressivité devenus inutiles sont libérés avant les étapes suivantes. Les allocations sont admises avant modification du document ; un refus conserve pixels et sélection. La limite globale de 18 Mio de Dessin reste inchangée. Comme en 0.8.50, l’allocateur peut libérer d’anciens états d’annulation sous pression mémoire ; un refus préserve le document courant, sans garantir la conservation de tous les anciens états. Les calculs avancent par étapes dans le mécanisme coopératif existant, avec annulation et historique.

Mesures du modèle exécuté sur ordinateur, qui ne sont pas des relevés du tas de la tablette :

| Cas | Pic suivi |
| --- | ---: |
| Relief réussi, 1 024 × 768, sans réserve de vue | 8 432 952 octets |
| Relief réussi, 1 100 × 610, réserve de vue de 2 Mio comprise | 10 813 296 octets |
| Refus contrôlé, 2 000 × 1 000, pixels conservés | 16 107 884 octets |
| Régression des déplacements répétés, réserve de vue comprise | 18 202 120 octets |

Le nouveau champ de distance consomme davantage que l’ancien champ sur un octet lorsqu’il couvre toute une image. Il est limité au rectangle concerné et ne reste pas alloué après l’opération. Les grandes images peuvent être refusées lorsque l’image, la vue, la sélection et l’historique épuisent le budget ; aucune réduction automatique ni augmentation du plafond ne masque ce cas.

Le pack russe passe de 523 250 à 519 596 octets, soit 4 692 octets de marge sous 512 Kio. Les 3 646 clés des quatorze packs sont vérifiées. La suppression du paragraphe obsolète conserve l’aide Explorateur actuelle et toutes les autres traductions existantes, hors changements terminologiques explicitement recensés.

## Vérifications exécutées

- Moteur du relief : ASan/UBSan, 48 scénarios de refus d’allocation, annulation à chaque étape, disque/anneau/concavités/régions séparées, progression en pixels et en proportion, signe, éclairage, alpha et annulation exacte.
- Revue indépendante : comparaison de 80 masques avec un calcul exhaustif de distance, dimensions extrêmes jusqu’à 8 192 × 64 dans les deux orientations et refus contrôlé d’une image 1 400 × 1 400. Aucun octet suivi encore alloué à la destruction.
- Régression de sélection : déplacements répétés, presse-papiers, alpha partiel, annulation/rétablissement et 552 scénarios de refus mémoire. Les comportements corrigés en 0.8.50 restent vérifiés.
- Interface native : LVGL 9.6 réel et allocateur privé du projet, menus au pointeur et au clavier, répétition des commandes, couleurs de surbrillance, boutons précis et cycle du relief. Les captures ont été examinées visuellement.
- Langues : chargement des quatorze packs par le lecteur C++ réel, comparaison des traductions, fichiers invalides, lecture interrompue et refus d’allocation ; ASan/UBSan et compteurs de libération. Génération et cohérence Factory/ressources contrôlées.

Les compteurs d’assertions dans les fichiers de preuve incluent des comparaisons pixel par pixel ou entrée par entrée ; ils ne représentent pas autant de scénarios distincts. LeakSanitizer n’est pas disponible dans cet environnement ; les tests utilisent des registres explicites des allocations. Les frontières matérielles sont simulées uniquement dans les tests et décrites dans leurs rapports.

## Compilation complète et bilan avant/après

Compilation complète ESP32-P4 réussie, code retour 0 ; les sources sont restées identiques avant et après compilation. Vérification de 1 426 objets, 2 038 fichiers sources et 1 926 copies Arduino. L’ELF réellement lié confirme la présence des adaptations DSI/audio, TLSF, cache et mémoire ; les partitions restent inchangées.

| Mesure statique, octets | 0.8.50 | 0.8.51 | Écart |
| --- | ---: | ---: | ---: |
| BIN applicatif | 6 103 872 | 6 102 688 | −1 184 |
| Capacité d’une partition OTA | 6 553 600 | 6 553 600 | 0 |
| Marge OTA | 449 728 | 450 912 | +1 184 |
| DATA + BSS | 93 976 | 94 040 | +64 |
| Code IRAM | 98 882 | 98 882 | 0 |
| Code TCM / données TCM | 794 / 64 | 794 / 64 | 0 |
| RTC sans initialisation | 1 200 | 1 200 | 0 |
| Frame GCC de `ie_job_step` | 112 | 128 | +16 |

Les sections statiques ne mesurent pas la mémoire interne/PSRAM libre au repos. La frame d’une fonction n’est pas la consommation cumulée d’une pile de tâche. Aucun chiffre de démarrage, charge CPU, latence ou fragmentation sur tablette n’est annoncé.

Le passage GCC de diagnostic sur les unités modifiées produit zéro avertissement pour le moteur et l’aide ; 51 avertissements pour l’interface, également présents dans la 0.8.50 recompilée avec les mêmes options. Leurs textes, hors emplacements, sont identiques : API LVGL dépréciées, indentation ancienne compacte, aide interne inutilisée et préprocesseur du SDK. Le projet n’est pas présenté comme dépourvu d’avertissements.

Tests finaux de l’interface : 9 632 assertions, 47 captures, cinq fermetures et 75 événements clavier, zéro allocation PSRAM suivie restante. Le pic de cette suite, 12 437 385 octets, est identique à celui de la suite 0.8.50 ; il n’inclut pas l’ensemble de l’OS. Le chargeur de langues réel passe 51 253 contrôles et restitue toutes ses allocations suivies.

Le BIN séparé est **applicatif OTA**, à utiliser via la mise à jour de Tab5OS ; ce n’est pas une image fusionnée à écrire à l’adresse zéro. SHA-256 : `4ec94d3e32595040b1620f02f270dc53a5bea2a93650447ad28064eb0fdeac3d`. Mettre à jour les ressources de langues et Factory fournies avec le firmware en suivant LISEZ_MOI.md, sans écraser les données personnelles.


## Points restant à vérifier sur l’appareil

La latence de calcul sur le P4, les échanges PSRAM/DSI, le rendu tactile et la stabilité sonore nécessitent une Tab5 physique. Aucun essai matériel n’est revendiqué. La procédure [TESTS_TAB5_0.8.51.md](TESTS_TAB5_0.8.51.md) couvre menus, trois profils, répétitions, annulation, mémoire et contrôle des anciens symptômes. Aucun défaut bloquant n’est connu dans les sources testées ; cela ne remplace pas cette validation matérielle.

Les preuves et leurs empreintes sont sous `tools/verification/*0851*`. Les méthodes détaillées sont dans `tools/audit-0851`, avec le relevé des fichiers modifiés et la comparaison à la référence livrée. Le guide [DESSIN.md](DESSIN.md) décrit l’utilisation ; [LISEZ_MOI.md](LISEZ_MOI.md) décrit la mise à jour en préservant les fichiers personnels.
