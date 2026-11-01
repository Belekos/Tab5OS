# Dessin — capture progressive de l'historique, 0.8.48

## Cause confirmée dans la version livrée .47

`ie_brush_begin` et `ie_warp_begin` appellent `clone_state` avant le premier pixel. L'état ne copie déjà plus les calques inactifs, mais recopie encore la totalité du calque actif. À 1280×720, cela impose une copie de 3 686 400 octets de pixels au premier clic, même pour un point. Cette rafale est démontrée dans le code ; son lien exact avec le flash physique nécessite les mesures écran sur Tab5.

## Correction

Les gestes pinceau, gomme et déformation réservent toujours leur historique complet avant de modifier le document. Les pixels réservés ne sont plus initialisés inutilement : le bloc de 256 pixels est capturé juste avant sa première mutation, puis marqué dans un petit bitmap placé à la fin de la même allocation. Le pinceau lit sa couleur de départ uniquement après cette capture. Aucun appel d'allocation supplémentaire n'est introduit pendant un geste.

Undo, redo et annulation échangent les seuls blocs capturés, au moyen d'un tampon de pile de 256 octets ; les métadonnées et masques de sélection gardent leur comportement antérieur. Le document actif garde tous ses calques et ne possède jamais de bitmap sparse. L'historique possède son buffer et le bitmap qui en est la queue ; sa libération est unique.

Les autres opérations et `ie_gesture_begin` générique conservent les instantanés complets. Les formats T5I/BMP restent sérialisés champ par champ : aucune modification de format ou de données utilisateur. Le changement de taille de la structure interne exige la recompilation du core et de l'éditeur ensemble, effectuée par la construction complète du projet.

## Mesures hôte reproduites

Sur un document 1280×720 sans masque de sélection, avec un puis deux calques :

| Mesure instrumentée | .47 | .48 |
|---|---:|---:|
| Octets copiés au début du geste | 3 686 768 | 384 |
| Octets initialisés à zéro au début | 450 | 900 |
| Copie au premier point de 2 pixels de diamètre | 0 | 2 048 |
| Réservation supplémentaire bitmap d'historique | 0 | 450 |

La mesure copie inclut la structure de taille hôte ; elle n'est pas une mesure de trafic réel PSRAM, de FPS ou de durée. La réservation maximale du buffer de pixels reste volontairement inchangée. Un undo d'un geste couvrant toute l'image recopie les blocs alors que l'ancien mécanisme échangeait les pointeurs : ce coût est déplacé hors du premier clic, pas éliminé pour tous les scénarios. Un masque complexe de sélection est encore copié intégralement.

## Vérifications exécutées

`python3 tools/drawing-tests-0848/run.py --build <répertoire-externe>` :

- 4 000 opérations mélangées : pixels, calques et sélection strictement identiques à .47 après chaque opération ; pinceaux, déformations, annulation, undo/redo, calques, géométrie, sélection et ajustements.
- Empoisonnement ASan de tous les pixels d'historique non capturés : 5 936 contrôles, dimensions 1×1, 257×3, 641×359 et 1280×720. Le test autorise l'écriture d'un bloc capturé ; toute lecture avant capture échoue. Aucune allocation pendant les gestes.
- Suite cœur existante : 34 777 contrôles, 400 cycles, 336 scénarios d'échec d'allocation ; zéro allocation suivie restante.
- Suite sélections/remplissages existante : 157 059 contrôles, dont 150 oracles de remplissage aléatoires et 72 injections de panne.

Rapport et empreintes : `tools/verification/drawing-core-0848.json`. L’éditeur natif avec LVGL 9.6 réel passe également 7 491 contrôles, 38 rendus et 5 fermetures : zéro octet PSRAM suivi restant (`image-native-0848.json`). Le core et l’éditeur compilent pour ESP32-P4 avec la chaîne épinglée (`drawing-target-0848.json`). Le core ne produit aucun diagnostic ; l’éditeur inchangé conserve les avertissements de dépréciation LVGL. Les tailles cible mesurées sont IEState 296→304 octets et IEDoc 560→568 ; la pile statique du seul `state_swap` passe de 320 à 656 octets. Les effets visuels doivent être retestés sur l'appareil ; cette correction ne permet pas, seule, d'affirmer que tous les flashs sont résolus.
