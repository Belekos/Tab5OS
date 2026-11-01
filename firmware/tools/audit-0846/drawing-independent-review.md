# Relecture indépendante — historique Dessin et rendu, 0.8.46

Cette relecture a été faite par l'auteur de la correction d'affichage, distinct de l'auteur des instantanés partiels de Dessin. Aucun fichier de production Dessin n'a été édité par le relecteur.

## Propriété des calques

`state_swap` transfère les pointeurs des calques inchangés depuis le document vivant vers l'état restauré et retire ces pointeurs de l'état qui devient historique. Le document courant possède donc toujours tous les calques ; un état partiel ne possède que le calque modifié. Les masques de sélection et les métadonnées restent propres à chaque état.

L'utilisation d'indices pour les calques omis exige que l'ordre soit restauré avant de revenir à un état partiel plus ancien. Les chemins réels ont été examinés : ajout, duplication, suppression, déplacement, fusion, style et redimensionnement utilisent un état complet ; le renommage natif appelle également `ie_begin`. Le changement du calque actif annule d'abord un geste ouvert. Le texte utilise un état complet. Les modifications structurelles sont ainsi traversées par les annulations avant que le transfert d'un ancien état partiel ait lieu. Aucune utilisation de calque nul n'a été trouvée dans ces parcours valides.

La fonction de bas niveau `ie_brush` exige déjà qu'un `begin` ait réussi avant le dessin. Les appelants natifs et ELF examinés respectent ce contrat ; une utilisation externe ignorant ce contrat n'est pas présentée comme une panne reproduite de l'interface.

Le cas proposé de document courant supérieur à 8 Mio incluant le masque ne peut pas être construit par les chemins publics de sélection et d'ajout de calque examinés, qui bornent cet ensemble. Il n'a donc pas motivé la suppression de la protection d'historique.

## Test différentiel supplémentaire

Les sources de référence sont extraites du ZIP 0.8.45 effectivement livré, empreinte vérifiée. Le même exécutable de scénario est compilé séparément avec le noyau 0.8.45 puis le noyau 0.8.46. Les allocations sont remplies de `0xa5`, pour ne pas supposer que la couverture paresseuse provient d'une mémoire initialement nulle.

5 000 opérations déterministes sont mélangées : 321 gestes pinceau, 257 gomme, 292 déformation, 311 texte, 275 annulations, 277 rétablissements, ainsi que création, duplication, déplacement, fusion, suppression, style, nom, redimensionnement et masque elliptique. Les gestes comprennent validation et annulation avec historique de rétablissement.

Après **chaque** opération, les deux versions donnent le même résultat de fonction, le même nombre d'entrées annuler/rétablir, les mêmes dimensions, calque actif, noms, styles, pixels et masque de sélection, et les mêmes octets d'une vue RGB565 rendue avec déplacement et zoom. Une vérification distincte de 4 608 pixels confirme les primaires RGB565 attendues : rouge `0xf800`, vert `0x07e0`, bleu `0x001f`.

Les compteurs hôte passent de 24 944 à 21 028 allocations sur la série complète. Les deux versions rendent zéro octet encore détenu au compteur après destruction. ASan et UBSan ne signalent aucune erreur. Il ne s'agit pas d'une mesure de fragmentation, du bus PSRAM ou du temps de traitement sur tablette ; cette série n'injecte pas de refus d'allocation et complète les tests de défaillance de l'auteur.

Empreinte de la séquence comparée : `71db6c53cb2fef2199a625b5ca71a43dbc51da3be2d97cd9c53734e1cd597eec`. Preuve et limites : `tools/verification/drawing-review-0846.json`. Scénario et exécuteur : `tools/drawing-review-0846`.

## Invalidation et ordre des couleurs

Les fonctions de rendu utilisent toujours les pixels document `0xAABBGGRR` et produisent les mêmes mots RGB565 pour LVGL. Le code natif `Tab5ImageEditor.cpp` n'est pas modifié dans cette optimisation : le dessin normal invalide les limites du trait ; le rendu complet reste destiné aux changements de vue ou d'état complet. Aucun nouveau rafraîchissement périodique ni changement d'ordre d'octets n'est introduit par l'historique partiel.

La conformité de ces résultats n'établit pas que le flash physique signalé au clic est supprimé.
