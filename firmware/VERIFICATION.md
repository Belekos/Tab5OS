# Vérification 0.8.53

La compilation complète ESP32-P4 et les contrôles de l’ELF et du BIN réussissent : 1 426 objets, 2 040 fichiers source et 1 928 copies Arduino contrôlés. La référence est la 0.8.52 livrée. La première compilation complète a réussi ; après correction de la seule notice du sketch, une passe incrémentale finale a de nouveau compilé et lié le firmware. Les manifestes avant/après de cette passe sont identiques et incluent la notice corrigée.

## Ressources mesurées

| Mesure, octets | 0.8.52 | 0.8.53 | Évolution |
| --- | ---: | ---: | ---: |
| BIN applicatif | 6 106 224 | 6 113 184 | +6 960 |
| Capacité de chaque partition OTA | 6 553 600 | 6 553 600 | 0 |
| Marge OTA | 447 376 | 440 416 | −6 960 |
| DATA + BSS internes | 94 128 | 91 508 | **−2 620** |
| IRAM texte | 98 882 | 98 882 | 0 |
| TCM texte | 794 | 794 | 0 |
| TCM données | 64 | 64 | 0 |
| RTC noinit | 1 200 | 1 200 | 0 |

Le gain SRAM statique **net est de 2 620 octets**, nouvelles fonctions incluses. Les cinq symboles contrôlés dans les ELF avant/après établissent un gain brut de 2 624 octets : réserve EPUB de 2 052 octets remplacée par un pointeur de 4 octets, tables JPEG de 384 octets et gains tuner de 192 octets placés en `.flash.rodata`.

L’index EPUB est désormais alloué à la demande en PSRAM, avec repli interne borné si nécessaire, puis libéré à la fermeture. Le repli consomme de nouveau la taille de l’index sur le tas interne pendant son utilisation. Ces sections liées ne mesurent ni le tas libre réel, ni sa fragmentation, ni le coût dynamique des nouveaux objets LVGL. Aucune mesure de démarrage, de latence ou de consommation sur Tab5 n’est revendiquée.

Les partitions sont identiques à la référence. Les 44 fichiers surveillés d’affichage, audio, mémoire et adaptations SDK restent identiques. Les contrôles du lien vérifient aussi les unités DSI/DMA, les adaptations audio, l’allocateur LVGL, la synchronisation de cache et l’allocateur Bluetooth réellement présents dans l’image.

## Contrôles exécutés

| Banc | Résultat et périmètre |
| --- | --- |
| Survol des menus contextuels | Défaut .52 reproduit ; 110 contrôles d’entrée souris, 2 742 contrôles de régression et 512 cycles sur LVGL réel |
| Explorateur | 232 contrôles : géométrie, sélection, actions images, états indisponibles ; comparaison avec .52 |
| Visualisateur | 822 contrôles, 100 cycles T5I, 12 scénarios de refus mémoire ; zéro octet suivi restant |
| Meshtastic | 148 contrôles, clavier explicite avec USB, saisie et fermeture ; 12 cycles de fermeture et 11 captures |
| Icônes | 130 contrôles, thèmes clair/sombre et tailles 44/56 pixels |
| Bandeau | 119 contrôles, 10 raccourcis sans défilement, 30 cycles ouverture/fermeture ; identité stable et focus lors des mises à jour |
| Revue indépendante du bandeau | 45 contrôles de persistance et 33 de planification du catalogue ; ancien format conservé et aucun rescan au repos déclenché par les écritures de cache |
| Index EPUB | 84 112 contrôles ; 1 004 allocations et 1 004 libérations, aucun octet suivi restant ; PSRAM, repli interne, double refus et reprise |
| JPEG / tuner | 1 440 encodages JPEG identiques octet par octet à .52 ; 64 032 scénarios de réglage avec traces I²C simulées identiques |
| Compilation cible ciblée | Meshtastic et ContextMenu : 30 avertissements comme sur .52, aucun ajout dans ce périmètre ; JPEG et tuner compilés sur cible |
| Diagnostic du programme principal final | Compilation réussie, 0 erreur et 315 avertissements conservés dans le journal ; aucune comparaison historique du main n’a été effectuée |
| Langues et ressources | 14 packs contrôlés, six clés ajoutées par pack, toutes les anciennes traductions et métadonnées conservées ; Factory .53 et ressources intégrées vérifiées |
| Préservation | 1 909 sources de production inchangées, 16 sources modifiées ou ajoutées explicitement liées à leurs preuves ; 3 132 fichiers de preuves historiques conservés |

Les bancs UI utilisent LVGL 9.6 et ASan/UBSan. Ils exécutent les contrôleurs de production et, lorsque nécessaire, des extraits vérifiés du programme principal. Les transports USB/Bluetooth, le stockage NVS, les scans fichiers et les lancements applicatifs sont simulés aux frontières indiquées dans leurs rapports. Le clavier du banc Meshtastic est un adaptateur LVGL, pas le rendu complet du clavier OS. Le visualisateur T5I utilise son parseur et son rendu réels ; le banc ne valide pas les pixels WebP, dont le décodeur est substitué. Les mesures d’allocations ne prouvent pas l’absence globale de fuite dans l’OS.

### Échec historique distingué

L’ancien test `tools/epub-pagination-tests/test_book_index.cpp` attend une coordonnée verticale 111 alors que le code de référence produit déjà 94. Le même échec a été reproduit sur .52 et .53. Il reste documenté dans `sram-0853.json` ; ce test historique n’est pas annoncé réussi. Le comportement de pagination n’a pas été changé pour satisfaire cette attente obsolète. Les contrôles actuels de l’index et de son cycle de vie passent.

## Traçabilité et essais restants

Les rapports courants sont dans `tools/verification/*-0853.json`, les journaux de compilation dans `tools/verification/build-0853` et le delta expliqué dans `tools/audit-0853/code-delta.json` et `.patch`. Les scripts de publication revérifient les empreintes des sources, des tests, des ressources et des artefacts, puis valident l’archive après extraction.

SHA-256 du BIN OTA : `a6a43f66e8cef604ffa8ea7680ae358ddf30ffa5752e543414072526100409fd`.

Aucun Tab5 physique utilisé. Restent à vérifier sur appareil : tactile/USB, appairage et émission Mesh, rendu écran, coexistence avec les médias, mémoire libre réelle, réception RF et conservation des préférences après coupure électrique. Suivre `TESTS_TAB5_0.8.53.md`. Le firmware livré est une image applicative OTA ; il ne doit pas être écrit à l’adresse zéro comme une image fusionnée.
