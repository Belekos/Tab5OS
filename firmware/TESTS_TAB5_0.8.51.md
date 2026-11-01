# Essais sur Tab5 — 0.8.51

Ces essais nécessitent votre tablette ; ils ne sont pas remplacés par les tests sur ordinateur. Utiliser une copie d’un projet pour les essais d’édition. Aucun enregistrement automatique n’est ajouté.

## Menus et commandes

1. Ouvrir Dessin, puis Fichier, Édition et Affichage : aucun titre ne doit être répété dans la liste. Couper, Coller et Supprimer doivent être accessibles dans Édition. Affichage ne doit plus proposer Options de l’outil ; la roue crantée reste disponible.
2. Avec la souris, déplacer le pointeur entre les lignes : la surbrillance doit suivre la commande visée. Répéter deux fois la même commande de zoom, puis deux annulations successives après deux traits. Chaque activation doit produire exactement une action.
3. Au clavier, ouvrir un menu, choisir avec les flèches, valider avec Entrée, puis répéter la même commande. Échap doit fermer sans agir. Vérifier aussi la sélection au doigt et le défilement d’une liste longue.
4. Refaire un passage en thème clair et sombre, puis dans une langue à intitulés longs. Les listes de réglage doivent continuer à signaler leur valeur sélectionnée.

## Relief

1. Sur un calque contenant une forme colorée, sélectionner successivement un disque, un anneau et une forme concave. Comparer les profils linéaire, sphérique et extrusion. Les trous et les zones hors sélection doivent rester intacts.
2. Pour les profils progressifs, comparer une faible distance en pixels et une proportion de 100 %. La première peut produire un plateau ; la seconde doit progresser jusqu’aux points les plus éloignés du contour de la région. Sur une forme dont les branches ont des largeurs différentes, les branches étroites peuvent rester moins hautes : ce réglage emploie la distance maximale globale.
3. Tester une hauteur positive puis négative, l’azimut aux quatre points cardinaux et près de 359°/0°, ainsi que l’intensité minimale et maximale. Le signe inverse la saillie et le creux. Le résultat reste un effet de relief dans une image, sans création de géométrie 3D.
4. Vérifier une bordure semi-transparente et un calque transparent : le relief doit conserver l’alpha et ne pas créer de matière dans les pixels transparents. Annuler puis rétablir doit restaurer exactement chaque état. Les réglages ne doivent pas modifier un document avant l’application de l’outil.

## Ressources et stabilité

1. Appliquer plusieurs reliefs, annuler/rétablir, changer d’outil, puis fermer et rouvrir Dessin. Vérifier la restitution des ressources dans les diagnostics mémoire, après stabilisation des tâches de fond.
2. Sur une copie de grande image avec plusieurs calques, vérifier qu’un éventuel refus mémoire conserve l’image et la sélection, puis qu’une autre opération reste possible. Le refus ne doit pas déclencher de redémarrage.
3. Vérifier brièvement lecture audio, navigation Média et repos à l’accueil : les corrections de Dessin ne doivent pas réintroduire les flashs ou claquements précédemment résolus.

Si un défaut apparaît, relever la version, les dimensions et le nombre de calques, le profil, l’étendue, la hauteur, l’azimut, l’intensité, le thème, la langue et le périphérique d’entrée. Joindre le journal série et les valeurs mémoire interne/PSRAM, minimum libre et plus grand bloc, avant puis après la reproduction. Ne pas inclure de secrets ni de fichiers personnels inutiles au diagnostic.
