# Dessin 0.8.50 — sélections, déplacement et presse-papiers

## Cause établie

Dans la 0.8.49, chaque déplacement effaçait la région courante puis prélevait les pixels de cette même région dans la copie d'annulation. Après le premier déplacement, ces pixels contenaient déjà le fond de destination : le déplacement suivant effaçait ce fond et transportait des pixels fusionnés. Un masque progressif subissait aussi des compositions répétées ; revenir à l'origine ne reconstituait pas les valeurs initiales.

Le témoin `tools/image-selection-tests-0850/regression.c` est compilé contre les sources de l'archive 0.8.49 authentifiée, puis contre la version livrée. Les deux écarts sont reproduits dans la référence et corrigés dans la nouvelle version.

## Propriété mémoire et comportement

- Le contenu initial reste immuable. Chaque position conserve le fond recouvert, restauré avant de déplacer ce contenu à nouveau. Les pixels du calque restent composés comme auparavant : aucun nouveau pipeline d'affichage.
- L'annulation et le rétablissement partagent par références les contenus et positions immuables. Une nouvelle modification de pixels ou une nouvelle sélection termine la sélection flottante ; l'annulation d'une modification retrouve sa position et son contenu antérieurs.
- Le retour à l'origine inverse exactement la découpe, y compris pour les couvertures fractionnaires, l'alpha partiel et les composantes RGB cachées des pixels transparents. Les pixels hors masque restent intacts.
- Un déplacement temporairement hors image ne détruit pas le contenu ; il reste possible de revenir dans le cadre. La sélection visible est bornée au document.
- Copier, couper, coller et supprimer sont des opérations du modèle. Supprimer un contenu flottant restitue le fond recouvert. Couper ne remplace le presse-papiers qu'après réussite de la suppression.
- Le presse-papiers est local à l'éditeur. Il survit à Nouveau et Ouvrir, indépendamment de l'historique. Les importeurs JPEG, PNG et WebP lui transfèrent la propriété seulement après validation complète du nouveau document. BMP et T5I remplacent l'état image sans détruire le presse-papiers. Fermer l'éditeur le libère.
- `ie_mask_stamp` applique un masque A8 borné dans une transaction existante pour le texte de l'OS. Le rendu bitmap historique reste disponible aux anciens appelants ; les formats de fichiers ne changent pas.

## Mémoire

Les limites existantes ne sont pas augmentées : moins de 8 Mio de pixels par document, 8 Mio d'instantanés raster et budget d'allocateur de l'éditeur de 18 Mio. Le contenu conservé utilise quatre octets par pixel, plus un octet uniquement lorsqu'un masque existe ; chaque position conservée ajoute un fond de quatre octets par pixel. Le contenu est partagé avec les historiques et le presse-papiers au lieu d'être recopié à chaque annulation.

La limite comptable de l'historique concerne les instantanés raster ; les données flottantes partagées sont néanmoins toutes facturées au même allocateur borné. L'éviction des anciens historiques suit le mécanisme existant. Toutes les allocations nécessaires sont admises avant de modifier les pixels. Un refus peut raccourcir l'historique pour tenter de récupérer sa mémoire, mais conserve les pixels, la sélection active et le presse-papiers.

Le scénario 1100 × 610 effectue 20 aller-retours avec annulation/rétablissement, en réservant 2 Mio pour une vue. Son pic d'allocateur est de **18 202 120 octets**, inférieur à 18 Mio. C'est une mesure hôte du modèle avec une réserve, pas une mesure de la mémoire totale ou de la latence de la Tab5. Les très grandes sélections, plusieurs calques et un grand presse-papiers peuvent épuiser ce budget et provoquer un refus propre ; aucune promesse de taille illimitée.

## Vérifications

Le manifeste `tools/verification/image-selection-0850.json` contient les résultats reconstruits et réellement exécutés, les empreintes des sources, le témoin avant/après et les limites de l'environnement. Il couvre :

- déplacements chevauchants, répétés, hors cadre et retour exact, couvertures 0/1/64/128/192/254/255 et alpha 0/85/170/255 ;
- annulation/rétablissement, suppression et découpe flottantes, reprise d'un déplacement après pinceau ou correction de couleur puis annulation ;
- presse-papiers conservé entre documents, imports BMP/T5I et transfert de propriété ;
- 216 scénarios de refus d'allocation ciblés, tests hérités du vrai modèle, destructions sans allocation suivie restante ;
- masque de texte partiel, coordonnées extrêmes et historique ;
- scénario grand document avec réserve de vue décrit ci-dessus.

ASan et UBSan sont actifs. LeakSanitizer n'est pas disponible dans cet environnement ; chaque destruction vérifie séparément l'absence d'allocation suivie restante. Les importations réelles PNG/JPEG/WebP et leurs interactions avec le presse-papiers sont vérifiées par la suite UI de cette version. La compilation cible et la compilation complète sont consignées dans les preuves d'intégration.

Sur Tab5 : sélectionner une zone avec transparence/progressivité, la déplacer plusieurs fois sur un fond contrasté, hors cadre puis à l'origine ; tester Annuler/Rétablir. Copier puis ouvrir une autre image de chaque format et coller, déplacer, supprimer. Vérifier le retour de la mémoire après fermeture de Dessin. Ces essais physiques ne sont pas revendiqués par les tests hôtes.
