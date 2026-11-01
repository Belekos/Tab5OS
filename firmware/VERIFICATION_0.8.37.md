# Vérification 0.8.37

Version préparée depuis le ZIP 0.8.36 livré, SHA-256 `6ec9ad6dda31e176938c625f361f1ca0d4b86122cc9b8ce9fc57d166dcd5ee78`. Les résultats ci-dessous portent sur les sources finales 0.8.37. Les anciens rapports conservent leur version d’origine. **Aucun essai sur le Tab5 physique n’a été effectué.**

## Firmware

Compilation complète Linux/GCC réussie avec Arduino M5Stack 3.3.9 et GCC RISC-V 14.2.0, sources inchangées pendant la compilation. La vérification contrôle l’ELF RISC-V 32 bits à ABI simple précision, l’image ESP32-P4, sa somme de contrôle, son empreinte de validation et l’empreinte ELF embarquée.

| Élément | Résultat |
|---|---|
| Image OTA | [Tab5OS_v0.8.37.bin](firmware/Tab5OS_v0.8.37.bin) |
| Taille | 6,072,560 octets |
| Partition OTA | 6,553,600 octets |
| Marge disponible | 481,040 octets, soit 469.8 Kio |
| SHA-256 | `03131278c997662ba1e512f1ae2ceeca8f05f512aab96543419e8162ef2c04aa` |
| Rapport | [firmware-verification-0837.json](tools/verification/firmware-verification-0837.json) |

Les preuves de compilation, empreintes de sources et dépendances figurent dans `tools/verification/build-0837`. Les 23 archives de dépendances et Arduino CLI ont été comparés au verrou SHA-256 fourni. Le BIN est une image applicative OTA, pas une image fusionnée à écrire à l’adresse zéro.

## Contrôles exécutés

| Périmètre | Résultat | Rapport |
|---|---|---|
| Sélection, ouverture et commandes de l’explorateur | PASS — 4,144 contrôles ; chemins de production extraits, widgets et focus LVGL réels, stockage et disponibilité de l’arbre simulés | [explorer-0837.json](tools/verification/explorer-0837.json) |
| Présentation et focus | PASS — 262 contrôles ; composants de l’explorateur et police livrée avec LVGL | [explorer-ui-0837.json](tools/verification/explorer-ui-0837.json) |
| Modèle d’arborescence | PASS — dépliage/repli, profondeur 129, 8 193 entrées, tri, annulation, suppression USB et refus d’allocation simulés | [explorer-tree-0837.json](tools/verification/explorer-tree-0837.json) |
| Chargement des langues | PASS — 74 contrôles ; chargeur de production, fichiers, préférences et sémaphores simulés | [locale-0837.json](tools/verification/locale-0837.json) |
| Parseur des langues | PASS — 96,160 contrôles ; comparaison différentielle sur les 14 packs et refus de données invalides | [locale-parser-0837.json](tools/verification/locale-parser-0837.json) |
| Routage de la souris Dessin | PASS — 9,046 contrôles ; route principale extraite et LVGL, frontières USB/éditeur simulées | [drawing-input-0837.json](tools/verification/drawing-input-0837.json) |
| Composants conservés | PASS — 1,843 fichiers de production identiques à la livraison 0.8.36 | [preserved-components-0837.json](tools/verification/preserved-components-0837.json) |
| Langues | PASS — 14 packs, couverture, paramètres de formatage, glyphes et limites de taille | [languages-0837.json](tools/verification/languages-0837.json) |
| Libellés nouveaux de l’explorateur | PASS — comparaison réelle des sources à la livraison 0.8.36, clés couvertes dans les 14 packs | [drawing-localization-0837.json](tools/verification/drawing-localization-0837.json) |
| Ressources SD et secours embarqué | PASS — 175 ressources Factory et 40 ressources embarquées, concordance des données et empreintes | [resources-0837.json](tools/verification/resources-0837.json) |

Les tests C/C++ ciblés emploient AddressSanitizer et UndefinedBehaviorSanitizer. LeakSanitizer ne peut pas inspecter tous les processus dans cet environnement ; les bancs qui suivent les allocations vérifient leur libération explicitement. Ces contrôles ne mesurent pas la cadence d’affichage du P4 ni le comportement de la microSD, du tactile ou de l’USB réels.

Le contrôle de préservation ne rejoue pas les essais historiques de Dessin, de Git, de mémoire, de Cartes ou des modificateurs USB. Il confirme l’identité des fichiers du périmètre défini, dont WriterUsb et Tab5Mouse ; il ne prouve pas à lui seul l’absence d’effet d’interaction sur tablette. L’explorateur, le chargeur de langue et le sketch d’intégration sont modifiés et possèdent des essais 0.8.37 distincts.

## Mesures sur ordinateur

Ces mesures comparent les algorithmes dans un banc hôte ; elles ne prédisent ni le temps de démarrage du P4, ni les lectures SD physiques, ni le nombre d’images par seconde.

| Mesure | Version précédente | Version corrigée | Conditions |
|---|---|---|---|
| Service de l’arbre au repos | 717.92 ms | 5.46 ms | 200000 appels, 512 dossiers développés, stockage simulé |
| Consultations du registre des éléments cachés pendant les recherches de ligne | 278560 | 0 | 32 recherches, 8705 lignes visibles |
| Analyse de `en-US.json` | 114.34 ms | 2.15 ms | Médiane de cinq exécutions optimisées, hors lecture du fichier |
| Analyse de `fr-FR.json` | 70.44 ms | 1.87 ms | Médiane de cinq exécutions optimisées, hors lecture du fichier |

Les journaux et conditions détaillées figurent dans les rapports d’arborescence et de parseur.

## Parcours et validation sur tablette

Le contrat et les défauts de la version précédente sont décrits dans [ERGONOMIE_0.8.37.md](ERGONOMIE_0.8.37.md). Vérifier le premier accès vide, le choix explicite de support, les clics dossier/texte/image/fichier inconnu depuis les deux panneaux et la correspondance entre chemin, contenu et commandes. Le double-clic droit révèle le dossier ou le parent du fichier à gauche, tout en conservant la cible initiale. Revenir depuis un aperçu, fermer Git, déplier des dossiers profonds/paginés et sélectionner avec Ctrl/Maj. Confirmer sur tablette la fluidité, le temps de chargement des langues et l’absence de scintillement.
