# Vérification 0.8.38

Version préparée depuis le ZIP 0.8.37 livré, SHA-256 `f8ec50004a841bc6d7f3502de240dd4eece58262f5360e00447e3ab47cb27a24`. Les essais 0.8.38 ci-dessous portent sur les sources finales. Les rapports antérieurs conservent leur version d’origine. **Aucun essai sur le Tab5 physique n’a été effectué.**

## Firmware

Compilation complète Linux/GCC réussie avec Arduino M5Stack 3.3.9 et GCC RISC-V 14.2.0, sources inchangées pendant la compilation. La vérification contrôle l’ELF RISC-V 32 bits à ABI simple précision, l’image ESP32-P4, sa somme de contrôle, son empreinte de validation et l’empreinte ELF embarquée.

| Élément | Résultat |
|---|---|
| Image OTA | [Tab5OS_v0.8.38.bin](firmware/Tab5OS_v0.8.38.bin) |
| Taille | 6,081,504 octets |
| Partition OTA | 6,553,600 octets |
| Marge disponible | 472,096 octets, soit 461.0 Kio |
| SHA-256 | `d32016bd8a13d066d5477dc96fc6b69c445f1eec708d9d6cfbea3629c9e09c7f` |
| Rapport | [firmware-verification-0838.json](tools/verification/firmware-verification-0838.json) |

Les preuves de compilation, empreintes de sources et dépendances figurent dans `tools/verification/build-0838`. Les 23 archives de dépendances et Arduino CLI ont été comparés au verrou SHA-256 fourni. Le BIN est une image applicative OTA, pas une image fusionnée à écrire à l’adresse zéro.

## Contrôles exécutés sur la 0.8.38

| Périmètre | Résultat | Rapport |
|---|---|---|
| Gestes, sélection et commandes de l’explorateur | PASS — 4,867 contrôles ; chemins de production extraits et LVGL réel, stockage et frontières applicatives simulés | [explorer-0838.json](tools/verification/explorer-0838.json) |
| Présentation et focus | PASS — 258 contrôles avec composants de production et police livrée | [explorer-ui-0838.json](tools/verification/explorer-ui-0838.json) |
| Arbre de dossiers et supports | PASS — modèle de production, simulations du stockage et des limites de mémoire ; portée détaillée dans le rapport | [explorer-tree-0838.json](tools/verification/explorer-tree-0838.json) |
| Noms de fichiers et dossiers par défaut | PASS — audit et essais des nouvelles valeurs anglaises, compatibilité des valeurs personnelles documentée | [english-defaults-0838.json](tools/verification/english-defaults-0838.json) |
| Routage de la souris Dessin | PASS — 9,046 contrôles ; route principale extraite et LVGL, frontières USB/éditeur simulées | [drawing-input-0838.json](tools/verification/drawing-input-0838.json) |
| Composants conservés | PASS — 1,841 fichiers de production identiques à la livraison 0.8.37 | [preserved-components-0838.json](tools/verification/preserved-components-0838.json) |
| Langues | PASS — 14 packs, couverture, paramètres de formatage, glyphes et limites de taille | [languages-0838.json](tools/verification/languages-0838.json) |
| Libellés modifiés | PASS — comparaison réelle à la livraison 0.8.37 et couverture dans les 14 packs | [drawing-localization-0838.json](tools/verification/drawing-localization-0838.json) |
| Ressources SD et secours embarqué | PASS — concordance de Factory, ressources SD et ressources intégrées | [resources-0838.json](tools/verification/resources-0838.json) |

Les tests C/C++ ciblés emploient AddressSanitizer et UndefinedBehaviorSanitizer lorsque précisé dans leur rapport. Les tests exécutés sur ordinateur ne mesurent pas les délais SD, la cadence d’affichage ou les entrées physiques du P4. Les images de test utilisent le rendu LVGL ; les données et certaines frontières applicatives y sont injectées.

## Preuves historiques conservées

Le chargeur et le parseur des langues ne sont pas réécrits dans cette version. Leurs sources sont contrôlées identiques à la 0.8.37 ; les rapports [locale-0837.json](tools/verification/locale-0837.json) et [locale-parser-0837.json](tools/verification/locale-parser-0837.json) restent des exécutions **0.8.37**, sans être présentés comme de nouveaux essais. Les mesures de vitesse de cette version restent historiques et ne prédisent pas le temps de démarrage réel du Tab5.

Le contrôle de préservation conserve aussi les résultats historiques des moteurs non modifiés. Il ne rejoue pas les essais de Git, de Dessin, de mémoire, de Cartes ou des modificateurs USB et ne prouve pas à lui seul l’absence d’interaction sur tablette. Les sources modifiées de l’explorateur, de l’intégration et des valeurs par défaut sont exclues explicitement de ce contrôle et possèdent leurs vérifications 0.8.38.

## Parcours et validation sur tablette

Les règles figurent dans [EXPLORATEUR.md](EXPLORATEUR.md) et [ERGONOMIE_0.8.38.md](ERGONOMIE_0.8.38.md). Vérifier la racine ouverte au premier accès, la conservation du dossier après retour à l’explorateur, l’absence de fichiers dans l’arbre gauche et l’apparition/retrait de la racine USB. À droite, un clic doit seulement sélectionner ; un double-clic ouvre. Contrôler les chevrons, les raccourcis de recherche/tri, les commandes d’Édition et les contextes Git. Les nouveaux noms de fichiers/dossiers doivent rester anglais quelle que soit la langue de l’interface, sans renommer les données personnelles existantes.
