# Vérification 0.8.42

Version préparée depuis le ZIP 0.8.41 livré, SHA-256 `01389322d10ae80b668ce369b2586e41831af03e3e407ecd3d60da62dd2667ff`. Les essais actuels portent sur les empreintes finales des sources. Les rapports antérieurs conservent leur version d’origine. **Aucun essai sur le Tab5 physique n’a été effectué.**

## Firmware

Compilation complète Linux/GCC réussie avec Arduino M5Stack 3.3.9 et GCC RISC-V 14.2.0, sources inchangées pendant la compilation. Le contrôle vérifie l’ELF RISC-V 32 bits à ABI simple précision, l’image ESP32-P4, ses empreintes et sa place dans la partition OTA.

| Élément | Résultat |
|---|---|
| Image OTA | [Tab5OS_v0.8.42.bin](firmware/Tab5OS_v0.8.42.bin) |
| Taille | 6,060,672 octets |
| Partition OTA | 6,553,600 octets |
| Marge | 492,928 octets |
| SHA-256 | `d8c91bb3dc3f3f16edbf71b179169024956bf1abe21401e2eddb292438970ba8` |
| Variation des sections data+BSS internes (0.8.42 − 0.8.41) | +768 octets |
| Rapport | [firmware-verification-0842.json](tools/verification/firmware-verification-0842.json) |

Les preuves de compilation figurent dans `tools/verification/build-0842`. Les 23 archives de dépendances et Arduino CLI sont contrôlés avec le verrou SHA-256 fourni. Le BIN est applicatif OTA, pas une image fusionnée à écrire à l’adresse zéro.

Les 14 424 octets des tables SHA1/XZ restent en flash en lecture seule, comme en 0.8.41 ; aucun nouveau gain RAM ne leur est attribué. La variation data+BSS ci-dessus inclut les nouvelles données statiques ; il ne constitue pas une mesure du tas libre après démarrage. Le vérificateur confirme aussi la provenance de l’allocateur Bluetooth et le relais effectif de l’interruption DSI, avec son chemin d’interruption en IRAM et ses compteurs en DRAM interne.

## Contrôles actuels

| Périmètre | Résultat | Rapport |
|---|---|---|
| Cycles de vie des tâches et services ELF | PASS | [apps-elf-lifecycle-0842.json](tools/verification/apps-elf-lifecycle-0842.json) |
| Chargement ELF : erreurs, annulation et transfert de propriété | PASS | [apps-elf-loader-0842.json](tools/verification/apps-elf-loader-0842.json) |
| Sauvegarde ELF : intégrité et fautes SD | PASS | [apps-elf-save-0842.json](tools/verification/apps-elf-save-0842.json) |
| Écriture image et JPEG : fautes SD/flush/close | PASS | [apps-image-io-0842.json](tools/verification/apps-image-io-0842.json) |
| Vue native : contention, entrées et libérations | PASS | [apps-native-view-0842.json](tools/verification/apps-native-view-0842.json) |
| Compilation cible des applications/exports image | PASS | [apps-target-0842.json](tools/verification/apps-target-0842.json) |
| Remplacement atomique et lecture JSON | PASS | [atomic-storage-0842.json](tools/verification/atomic-storage-0842.json) |
| Framebuffer/cache/DSI et reprise | PASS — 2771204 contrôles | [display-0842.json](tools/verification/display-0842.json) |
| Comparaison latence affichage et survol .41/.42 | PASS | [display-latency-0842.json](tools/verification/display-latency-0842.json) |
| Compilation cible écran/souris | PASS | [display-modules-target-0842.json](tools/verification/display-modules-target-0842.json) |
| Compilation cible du main régénéré | PASS | [main-target-0842.json](tools/verification/main-target-0842.json) |
| Cartes/souris : navigation, boutons et zoom | PASS — 9092 contrôles | [maps-input-0842.json](tools/verification/maps-input-0842.json) |
| Audio : accesseurs et copies de chaînes | PASS | [media-audio-accessors-after-0842.json](tools/verification/media-audio-accessors-after-0842.json) |
| Synthèse des contrôles réseau et média | PASS | [media-audit-0842.json](tools/verification/media-audit-0842.json) |
| Tri du catalogue média | PASS | [media-catalogue-sort-0842.json](tools/verification/media-catalogue-sort-0842.json) |
| Compatibilité C6 sans refus automatique | PASS — 45862 contrôles | [media-compatibility-0842.json](tools/verification/media-compatibility-0842.json) |
| HTTP : trames tronquées et fins chunked | PASS | [media-http-after-0842.json](tools/verification/media-http-after-0842.json) |
| Synchronisation et annulation réseau | PASS | [media-network-gate-0842.json](tools/verification/media-network-gate-0842.json) |
| Enregistrement : erreurs et conservation du fichier | PASS | [media-recording-0842.json](tools/verification/media-recording-0842.json) |
| Compilation cible audio/vidéo/HTTP | PASS | [media-target-0842.json](tools/verification/media-target-0842.json) |
| Vidéo : arrêt, détachement et relance | PASS | [media-video-lifecycle-0842.json](tools/verification/media-video-lifecycle-0842.json) |
| Admission mémoire, quotas et diagnostics | PASS | [memory-0842.json](tools/verification/memory-0842.json) |
| Compilation cible mémoire | PASS | [memory-target-0842.json](tools/verification/memory-target-0842.json) |
| Paramètres et progression : lectures/écritures interrompues | PASS | [persistence-0842.json](tools/verification/persistence-0842.json) |
| Réglages et informations système : LVGL, 14 langues | PASS | [settings-ui-0842.json](tools/verification/settings-ui-0842.json) |
| Compilation cible coffre et clés | PASS | [storage-target-0842.json](tools/verification/storage-target-0842.json) |
| Volumes/leases et génération des handles | PASS | [storage-volume-0842.json](tools/verification/storage-volume-0842.json) |
| Compilation cible persistance et stockage | PASS | [storage2-target-0842.json](tools/verification/storage2-target-0842.json) |
| Réparation : reconstruction, mémoire et fautes SD | PASS | [system-resources-0842.json](tools/verification/system-resources-0842.json) |
| Compilation cible des ressources compressées | PASS | [system-resources-target-0842.json](tools/verification/system-resources-target-0842.json) |
| Diagnostic UI : horloge, bornes et journal RTC | PASS — 967 contrôles | [ui-diagnostics-0842.json](tools/verification/ui-diagnostics-0842.json) |
| Compilation cible du diagnostic UI | PASS | [ui-diagnostics-target-0842.json](tools/verification/ui-diagnostics-target-0842.json) |
| Coffre : vraie crypto, récupération et fautes | PASS | [vault-0842.json](tools/verification/vault-0842.json) |
| Documents/workspace : cycles et conservation sur erreur | PASS | [writer-storage-0842.json](tools/verification/writer-storage-0842.json) |
| Composants hors périmètre modifié | PASS — 1,876 fichiers identiques à la livraison 0.8.41 ; chaque source modifiée est déclarée avec son rapport actuel | [preserved-components-0842.json](tools/verification/preserved-components-0842.json) |
| Langues | PASS — 14 packs, paramètres de formatage, glyphes, couverture et limites de taille | [languages-0842.json](tools/verification/languages-0842.json) |
| Nouveaux libellés | PASS — comparaison au ZIP 0.8.41, traductions existantes conservées | [evolutions-localization-0842.json](tools/verification/evolutions-localization-0842.json) |
| Ressources SD et secours intégré | PASS — concordance des fichiers, de Factory et des ressources embarquées | [resources-0842.json](tools/verification/resources-0842.json) |

Les rapports détaillent les adaptateurs et simulations utilisés, les limites mémoire testées et les éventuels sanitizers. Un test hôte n’est pas un essai d’impression, de formatage ou de connexion radio sur un appareil réel. Le registre `tools/release_scope_0842.json` fixe les rapports attendus et les changements de code explicitement couverts ; l’archive est réextraite et revalidée avant livraison.

## Historique et limites

Les preuves 0.8.41 et antérieures restent historiques. La conservation d’une source ne prouve pas à elle seule l’absence d’interaction avec une nouvelle fonction. Aucun temps de démarrage, débit USB, portée radio ni disparition du scintillement sur tablette n’est déduit de la compilation.

La portée livrée et les développements qui nécessitent encore un pilote, une adaptation d’ABI ou un matériel externe sont décrits dans [NOUVEAUTES_v0.8.42.md](NOUVEAUTES_v0.8.42.md) et [EVOLUTIONS_0.8.39.md](EVOLUTIONS_0.8.39.md). Le diagnostic technique, les ressources et les problèmes résiduels figurent dans [AUDIT_0.8.42.md](AUDIT_0.8.42.md), [BILAN_RESSOURCES_0.8.42.md](BILAN_RESSOURCES_0.8.42.md) et [TESTS_TAB5_0.8.42.md](TESTS_TAB5_0.8.42.md).
