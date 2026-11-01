# Vérification 0.8.40

Version préparée depuis le ZIP 0.8.39 livré, SHA-256 `a4274b8fb44642306dc9c78e212d58d9064147d9bbdb0e624b4afb612af7d5ff`. Les essais actuels portent sur les empreintes finales des sources. Les rapports antérieurs conservent leur version d’origine. **Aucun essai sur le Tab5 physique n’a été effectué.**

## Firmware

Compilation complète Linux/GCC réussie avec Arduino M5Stack 3.3.9 et GCC RISC-V 14.2.0, sources inchangées pendant la compilation. Le contrôle vérifie l’ELF RISC-V 32 bits à ABI simple précision, l’image ESP32-P4, ses empreintes et sa place dans la partition OTA.

| Élément | Résultat |
|---|---|
| Image OTA | [Tab5OS_v0.8.40.bin](firmware/Tab5OS_v0.8.40.bin) |
| Taille | 6,294,496 octets |
| Partition OTA | 6,553,600 octets |
| Marge | 259,104 octets |
| SHA-256 | `35392e6c4784f195b3ba91453d2244731ba7cdf4c8429502193ab7cc496bf5a0` |
| Rapport | [firmware-verification-0840.json](tools/verification/firmware-verification-0840.json) |

Les preuves de compilation figurent dans `tools/verification/build-0840`. Les 23 archives de dépendances et Arduino CLI sont contrôlés avec le verrou SHA-256 fourni. Le BIN est applicatif OTA, pas une image fusionnée à écrire à l’adresse zéro.

## Contrôles actuels

| Périmètre | Résultat | Rapport |
|---|---|---|
| Réglages, fenêtre Polices, informations système et intégration shell avec LVGL réel | PASS | [settings-ui-0840.json](tools/verification/settings-ui-0840.json) |
| Repères Cartes et notification Mesh avec LVGL réel | PASS — 106922 contrôles | [map-overlay-0840.json](tools/verification/map-overlay-0840.json) |
| Colorations par langage, contexte et persistance du choix | PASS — 9841 contrôles | [editor-syntax-0840.json](tools/verification/editor-syntax-0840.json) |
| Masques de sélection, filtres, remplissage et annulation | PASS — 54433 contrôles | [drawing-core-0840.json](tools/verification/drawing-core-0840.json) |
| Dessin intégré, événements et captures LVGL | PASS — 2297 contrôles | [image-native-0840.json](tools/verification/image-native-0840.json) |
| Recherche locale bornée, annulation, résultats et positions Mesh | PASS — 10699 contrôles | [maps-0840.json](tools/verification/maps-0840.json) |
| Reconstruction identique du gazetteer GeoNames | PASS — 18463 contrôles | [maps-generator-0840.json](tools/verification/maps-generator-0840.json) |
| Compilation ESP32-P4 du moteur Cartes | PASS | [maps-target-0840.json](tools/verification/maps-target-0840.json) |
| Protocole, persistance, mémoire et interface Meshtastic | PASS | [meshtastic-0840.json](tools/verification/meshtastic-0840.json) |
| Notification finale USB et cycles FAT réels | PASS — 787949 contrôles | [usb-format-0840.json](tools/verification/usb-format-0840.json) |
| Catalogue ELF dynamique, parcours bornés et erreurs | PASS | [home-catalogue-0840.json](tools/verification/home-catalogue-0840.json) |
| Compteurs passifs, publication des tampons et erreurs écran | PASS — 9578 contrôles | [display-0840.json](tools/verification/display-0840.json) |
| Chargement de production et traductions des 14 langues | PASS — 50357 contrôles | [locale-0840.json](tools/verification/locale-0840.json) |
| États de recherche Cartes, annulations et interactions des fenêtres | PASS — 18 contrôles | [maps-ui-0840.json](tools/verification/maps-ui-0840.json) |
| Compilation ESP32-P4 et usages de pile Meshtastic | PASS | [meshtastic-target-0840.json](tools/verification/meshtastic-target-0840.json) |
| Composants hors périmètre modifié | PASS — 1,849 fichiers identiques à la livraison 0.8.39 ; chaque source modifiée est déclarée avec son rapport actuel | [preserved-components-0840.json](tools/verification/preserved-components-0840.json) |
| Langues | PASS — 14 packs, paramètres de formatage, glyphes, couverture et limites de taille | [languages-0840.json](tools/verification/languages-0840.json) |
| Nouveaux libellés | PASS — comparaison au ZIP 0.8.39, traductions existantes conservées | [evolutions-localization-0840.json](tools/verification/evolutions-localization-0840.json) |
| Ressources SD et secours intégré | PASS — concordance des fichiers, de Factory et des ressources embarquées | [resources-0840.json](tools/verification/resources-0840.json) |

Les rapports détaillent les adaptateurs et simulations utilisés, les limites mémoire testées et les éventuels sanitizers. Un test hôte n’est pas un essai d’impression, de formatage ou de connexion radio sur un appareil réel. Le registre `tools/release_scope_0840.json` fixe les rapports attendus et les changements de code explicitement couverts ; l’archive est réextraite et revalidée avant livraison.

## Historique et limites

Les preuves 0.8.39 et antérieures restent historiques. La conservation d’une source ne prouve pas à elle seule l’absence d’interaction avec une nouvelle fonction. Aucun temps de démarrage, débit USB, portée radio ni disparition du scintillement sur tablette n’est déduit de la compilation.

La portée livrée et les développements qui nécessitent encore un pilote, une adaptation d’ABI ou un matériel externe sont décrits dans [NOUVEAUTES_v0.8.40.md](NOUVEAUTES_v0.8.40.md) et [EVOLUTIONS_0.8.39.md](EVOLUTIONS_0.8.39.md). Les priorités 1, 2 et 3 représentent la difficulté de réalisation, sans jugement sur la valeur des fonctions.
