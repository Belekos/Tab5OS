# Vérification 0.8.39

Version préparée depuis le ZIP 0.8.38 livré, SHA-256 `79fac4f6ac0858ac61378939388a988ceed03dfd57ae69bec7f5f8154b966360`. Les essais actuels portent sur les empreintes finales des sources. Les rapports antérieurs conservent leur version d’origine. **Aucun essai sur le Tab5 physique n’a été effectué.**

## Firmware

Compilation complète Linux/GCC réussie avec Arduino M5Stack 3.3.9 et GCC RISC-V 14.2.0, sources inchangées pendant la compilation. Le contrôle vérifie l’ELF RISC-V 32 bits à ABI simple précision, l’image ESP32-P4, ses empreintes et sa place dans la partition OTA.

| Élément | Résultat |
|---|---|
| Image OTA | [Tab5OS_v0.8.39.bin](firmware/Tab5OS_v0.8.39.bin) |
| Taille | 6,193,792 octets |
| Partition OTA | 6,553,600 octets |
| Marge | 359,808 octets |
| SHA-256 | `3215ee357ee4e9c2599bc71c04584132522d0c0ccf52a47d01901c4be3b9a6cc` |
| Rapport | [firmware-verification-0839.json](tools/verification/firmware-verification-0839.json) |

Les preuves de compilation figurent dans `tools/verification/build-0839`. Les 23 archives de dépendances et Arduino CLI sont contrôlés avec le verrou SHA-256 fourni. Le BIN est applicatif OTA, pas une image fusionnée à écrire à l’adresse zéro.

## Contrôles actuels

| Périmètre | Résultat | Rapport |
|---|---|---|
| Rotation paysage/portrait, formatage et Dessin /Picture | PASS — 19386 contrôles | [root-ui-0839.json](tools/verification/root-ui-0839.json) |
| Validation et budget des polices TrueType | PASS — 32990 contrôles | [font-defaults-0839.json](tools/verification/font-defaults-0839.json) |
| Intégration LVGL et polices CJK | PASS — 246 contrôles | [font-integration-0839.json](tools/verification/font-integration-0839.json) |
| Choix natif, ELF et Python, confirmation et USB | PASS — 79 contrôles | [open-with-0839.json](tools/verification/open-with-0839.json) |
| Encodages texte, C et JSON | PASS — 7973 contrôles | [encoding-editor-0839.json](tools/verification/encoding-editor-0839.json) |
| Contrôleur et formatage USB FAT/FAT32 | PASS — 776952 contrôles | [usb-format-0839.json](tools/verification/usb-format-0839.json) |
| Transport imprimante IPP-over-USB | PASS — 72370 contrôles | [usb-ipp-0839.json](tools/verification/usb-ipp-0839.json) |
| Protocole, états et interface Meshtastic | PASS | [meshtastic-0839.json](tools/verification/meshtastic-0839.json) |
| Chargement réel et toutes les traductions des 14 langues | PASS — 49041 contrôles | [locale-0839.json](tools/verification/locale-0839.json) |
| Composants hors périmètre modifié | PASS — 1,828 fichiers identiques à la livraison 0.8.38 ; chaque source modifiée est déclarée avec son rapport actuel | [preserved-components-0839.json](tools/verification/preserved-components-0839.json) |
| Langues | PASS — 14 packs, paramètres de formatage, glyphes, couverture et limites de taille | [languages-0839.json](tools/verification/languages-0839.json) |
| Nouveaux libellés | PASS — comparaison au ZIP 0.8.38, traductions existantes conservées | [evolutions-localization-0839.json](tools/verification/evolutions-localization-0839.json) |
| Ressources SD et secours intégré | PASS — concordance des fichiers, de Factory et des ressources embarquées | [resources-0839.json](tools/verification/resources-0839.json) |

Les rapports détaillent les adaptateurs et simulations utilisés, les limites mémoire testées et les éventuels sanitizers. Un test hôte n’est pas un essai d’impression, de formatage ou de connexion radio sur un appareil réel. Le registre `tools/release_scope_0839.json` fixe les rapports attendus et les changements de code explicitement couverts ; l’archive est réextraite et revalidée avant livraison.

## Historique et limites

Les preuves 0.8.38 et antérieures restent historiques. La conservation d’une source ne prouve pas à elle seule l’absence d’interaction avec une nouvelle fonction. Aucun temps de démarrage, débit USB, portée radio ni disparition du scintillement sur tablette n’est déduit de la compilation.

La portée livrée et les développements qui nécessitent encore un pilote, une adaptation d’ABI ou un matériel externe sont décrits dans [NOUVEAUTES_v0.8.39.md](NOUVEAUTES_v0.8.39.md) et [EVOLUTIONS_0.8.39.md](EVOLUTIONS_0.8.39.md). Les priorités 1, 2 et 3 représentent la difficulté de réalisation, sans jugement sur la valeur des fonctions.
