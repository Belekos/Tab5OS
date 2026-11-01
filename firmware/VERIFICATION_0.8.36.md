# Vérification 0.8.36

Version préparée depuis le ZIP 0.8.35 livré, SHA-256 `fc3c89c9e84acc6e50d29025081a1cfdb1a57437518c15ee36e77a7b6b160d47`. Les résultats ci-dessous portent sur les sources finales 0.8.36. Les anciens rapports conservent leur version d’origine. **Aucun essai sur le Tab5 physique n’a été effectué.**

## Firmware

Compilation complète Linux/GCC réussie avec Arduino M5Stack 3.3.9 et GCC RISC-V 14.2.0, sources inchangées pendant la compilation. La vérification contrôle l’ELF RISC-V 32 bits à ABI simple précision, l’image ESP32-P4, sa somme de contrôle, son empreinte de validation et l’empreinte ELF embarquée.

| Élément | Résultat |
|---|---|
| Image OTA | [Tab5OS_v0.8.36.bin](firmware/Tab5OS_v0.8.36.bin) |
| Taille | 6,065,536 octets |
| Partition OTA | 6,553,600 octets |
| Marge disponible | 488,064 octets, soit 476.6 Kio |
| SHA-256 | `b5b752029624e40203721d182748cb127fb219794c1a3fd69c69bc8258956ca8` |
| Rapport | [firmware-verification-0836.json](tools/verification/firmware-verification-0836.json) |

Les preuves de compilation, empreintes de sources et dépendances figurent dans `tools/verification/build-0836`. Les 23 archives de dépendances et Arduino CLI ont été comparés au verrou SHA-256 fourni. Le BIN est une image applicative OTA, pas une image fusionnée à écrire à l’adresse zéro.

## Contrôles exécutés

| Périmètre | Résultat | Rapport |
|---|---|---|
| Sélection, ouverture et commandes de l’explorateur | PASS — 4,592 contrôles ; chemins de production extraits, widgets et focus LVGL réels, stockage et disponibilité de l’arbre simulés | [explorer-0836.json](tools/verification/explorer-0836.json) |
| Présentation et focus | PASS — 1,162 contrôles ; composants de l’explorateur et police livrée avec LVGL | [explorer-ui-0836.json](tools/verification/explorer-ui-0836.json) |
| Modèle d’arborescence | PASS — dépliage/repli, profondeur 129, 8 193 entrées, tri, annulation, suppression USB et refus d’allocation simulés | [explorer-tree-0836.json](tools/verification/explorer-tree-0836.json) |
| Modificateurs du clavier USB | PASS — 24,828 contrôles ; huit interfaces, pressions/relâchements, débranchement et débordement de file | [usb-modifiers-0836.json](tools/verification/usb-modifiers-0836.json) |
| Routage de la souris Dessin | PASS — 9,046 contrôles ; route principale extraite et LVGL, frontières USB/éditeur simulées | [drawing-input-0836.json](tools/verification/drawing-input-0836.json) |
| Composants conservés | PASS — 1,845 fichiers de production identiques à la livraison 0.8.35 | [preserved-components-0836.json](tools/verification/preserved-components-0836.json) |
| Langues | PASS — 14 packs, couverture, paramètres de formatage, glyphes et limites de taille | [languages-0836.json](tools/verification/languages-0836.json) |
| Libellés nouveaux de l’explorateur | PASS — comparaison réelle des sources à la livraison 0.8.35, clés couvertes dans les 14 packs | [drawing-localization-0836.json](tools/verification/drawing-localization-0836.json) |
| Ressources SD et secours embarqué | PASS — 175 ressources Factory et 40 ressources embarquées, concordance des données et empreintes | [resources-0836.json](tools/verification/resources-0836.json) |

Les tests C/C++ ciblés emploient AddressSanitizer et UndefinedBehaviorSanitizer. LeakSanitizer ne peut pas inspecter tous les processus dans cet environnement ; les bancs qui suivent les allocations vérifient leur libération explicitement. Ces contrôles ne mesurent pas la cadence d’affichage du P4 ni le comportement de la microSD, du tactile ou de l’USB réels.

Le contrôle de préservation ne rejoue pas les essais historiques de Dessin, de Git, de mémoire ou de Cartes. Il confirme l’identité des fichiers du périmètre défini ; il ne prouve pas à lui seul l’absence d’effet d’interaction sur tablette. L’intégration de l’explorateur dans le sketch principal est modifiée, notamment pour rendre les métadonnées de l’arbre lorsque la récupération explicite du cache Fichiers concerne l’application inactive ; aucun nettoyage périodique n’est ajouté.

## Validation sur tablette

Vérifier un clic simple sur un dossier à droite, puis un double-clic : le premier ouvre uniquement à droite ; le second révèle aussi le dossier à gauche. Essayer des dossiers profonds et paginés, les chevrons, Ctrl/Maj, la fermeture des menus, puis les actions copier/coller/Git depuis chaque panneau. La cible doit toujours correspondre à la dernière sélection. Confirmer également la fluidité et l’absence de flash en usage réel.
