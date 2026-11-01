# Vérification 0.8.41

Version préparée depuis le ZIP 0.8.40 livré, SHA-256 `8ff7603e05bed90463d2959aa821193511bae63458761b1b7304f62e106bdcd7`. Les essais actuels portent sur les empreintes finales des sources. Les rapports antérieurs conservent leur version d’origine. **Aucun essai sur le Tab5 physique n’a été effectué.**

## Firmware

Compilation complète Linux/GCC réussie avec Arduino M5Stack 3.3.9 et GCC RISC-V 14.2.0, sources inchangées pendant la compilation. Le contrôle vérifie l’ELF RISC-V 32 bits à ABI simple précision, l’image ESP32-P4, ses empreintes et sa place dans la partition OTA.

| Élément | Résultat |
|---|---|
| Image OTA | [Tab5OS_v0.8.41.bin](firmware/Tab5OS_v0.8.41.bin) |
| Taille | 6,310,368 octets |
| Partition OTA | 6,553,600 octets |
| Marge | 243,232 octets |
| SHA-256 | `55d2edd0e12aaffb7b4435e2b805258f958e6c23c77eda13abe8c29896b7202a` |
| Gain statique des sections data+BSS internes depuis 0.8.40 | 13,872 octets |
| Rapport | [firmware-verification-0841.json](tools/verification/firmware-verification-0841.json) |

Les preuves de compilation figurent dans `tools/verification/build-0841`. Les 23 archives de dépendances et Arduino CLI sont contrôlés avec le verrou SHA-256 fourni. Le BIN est applicatif OTA, pas une image fusionnée à écrire à l’adresse zéro.

Les 14 424 octets des tables SHA1/XZ sont en flash en lecture seule dans l’ELF final. Le gain data+BSS ci-dessus tient compte des nouvelles données statiques ; il ne constitue pas une mesure du tas libre après démarrage. Le vérificateur confirme aussi la provenance de l’allocateur Bluetooth et le relais effectif de l’interruption DSI, avec son chemin d’interruption en IRAM et ses compteurs en DRAM interne.

## Contrôles actuels

| Périmètre | Résultat | Rapport |
|---|---|---|
| Cartes : recherche locale, files PSRAM et calcul du zoom | PASS — 540852 contrôles | [maps-0841.json](tools/verification/maps-0841.json) |
| Routage réel de la molette, focus et contextes de l’interface | PASS — 9092 contrôles | [maps-input-0841.json](tools/verification/maps-input-0841.json) |
| Recherche Cartes extraite de l’interface finale | PASS — 18 contrôles | [maps-ui-0841.json](tools/verification/maps-ui-0841.json) |
| Préservation du canevas Cartes : reproduction .40 et correction .41 | PASS | [maps-canvas-0841.json](tools/verification/maps-canvas-0841.json) |
| Séparation HP, SPM/TCM, LP et PSRAM sans double compte | PASS | [memory-layout-0841.json](tools/verification/memory-layout-0841.json) |
| Tables SHA1/XZ en lecture seule : calculs et collisions | PASS | [memory-static-0841.json](tools/verification/memory-static-0841.json) |
| Métadonnées du firmware : cache, bornes et erreurs | PASS | [firmware-footprint-0841.json](tools/verification/firmware-footprint-0841.json) |
| Lecture des métadonnées compilée pour P4 et contrat SDK | PASS | [firmware-footprint-target-0841.json](tools/verification/firmware-footprint-target-0841.json) |
| Affichage : pipeline réel, repos simulé et relais d’interruption DSI | PASS — 3671481 contrôles | [display-0841.json](tools/verification/display-0841.json) |
| Meshtastic : mémoire, protocole, historique et interface | PASS | [meshtastic-0841.json](tools/verification/meshtastic-0841.json) |
| Compatibilité de l’allocateur hôte avec le SDK fourni | PASS | [meshtastic-sdk-0841.json](tools/verification/meshtastic-sdk-0841.json) |
| Sources Meshtastic et allocateur compilés pour P4 | PASS | [meshtastic-target-0841.json](tools/verification/meshtastic-target-0841.json) |
| Dessin : progressivité intérieure, combinaison et allocations | PASS — 157059 contrôles | [drawing-core-0841.json](tools/verification/drawing-core-0841.json) |
| Dessin : interface réelle, lasso, remplissage et annulation | PASS — 7492 contrôles | [image-native-0841.json](tools/verification/image-native-0841.json) |
| Réglages et informations système : 14 langues, géométrie et mémoire | PASS | [settings-ui-0841.json](tools/verification/settings-ui-0841.json) |
| Pastille Mesh et titre du bandeau supérieur | PASS — 806 contrôles | [badge-0841.json](tools/verification/badge-0841.json) |
| Composants hors périmètre modifié | PASS — 1,878 fichiers identiques à la livraison 0.8.40 ; chaque source modifiée est déclarée avec son rapport actuel | [preserved-components-0841.json](tools/verification/preserved-components-0841.json) |
| Langues | PASS — 14 packs, paramètres de formatage, glyphes, couverture et limites de taille | [languages-0841.json](tools/verification/languages-0841.json) |
| Nouveaux libellés | PASS — comparaison au ZIP 0.8.40, traductions existantes conservées | [evolutions-localization-0841.json](tools/verification/evolutions-localization-0841.json) |
| Ressources SD et secours intégré | PASS — concordance des fichiers, de Factory et des ressources embarquées | [resources-0841.json](tools/verification/resources-0841.json) |

Les rapports détaillent les adaptateurs et simulations utilisés, les limites mémoire testées et les éventuels sanitizers. Un test hôte n’est pas un essai d’impression, de formatage ou de connexion radio sur un appareil réel. Le registre `tools/release_scope_0841.json` fixe les rapports attendus et les changements de code explicitement couverts ; l’archive est réextraite et revalidée avant livraison.

## Historique et limites

Les preuves 0.8.40 et antérieures restent historiques. La conservation d’une source ne prouve pas à elle seule l’absence d’interaction avec une nouvelle fonction. Aucun temps de démarrage, débit USB, portée radio ni disparition du scintillement sur tablette n’est déduit de la compilation.

La portée livrée et les développements qui nécessitent encore un pilote, une adaptation d’ABI ou un matériel externe sont décrits dans [NOUVEAUTES_v0.8.41.md](NOUVEAUTES_v0.8.41.md) et [EVOLUTIONS_0.8.39.md](EVOLUTIONS_0.8.39.md). Les priorités 1, 2 et 3 représentent la difficulté de réalisation, sans jugement sur la valeur des fonctions.
