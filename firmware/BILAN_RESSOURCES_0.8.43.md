# Bilan des ressources — Tab5OS 0.8.43

La référence est le BIN/ELF de la 0.8.42 livrée. Les tailles finales viennent du firmware 0.8.43 réellement compilé, dont les sources sont restées identiques pendant le build. Aucun tas libre au démarrage, temps CPU, énergie, débit SD ou fréquence d’image sur matériel n’est déduit de ces chiffres.

## Flash et OTA

| Mesure | 0.8.42 | 0.8.43 |
|---|---:|---:|
| Firmware OTA | 6 060 672 octets | 6 062 352 octets |
| Capacité d’un slot | 6 553 600 octets | 6 553 600 octets |
| Marge | 492 928 octets | 491 248 octets |
| Occupation | 92,48 % | 92,50 % |

Le BIN augmente de **1 680 octets** : worker, synchronisation, version des ressources et changements ciblés ont un coût. Le nettoyage de code inutilisé ne garantit pas une diminution du binaire, car le linker éliminait déjà certains éléments non référencés. Aucune fonctionnalité n’est sacrifiée pour obtenir un meilleur chiffre. La table des partitions est strictement identique et aucune migration n’est nécessaire.

## Sections internes du lien

| Section | Référence, octets | Nouveau, octets | Nouveau−ancien, octets |
|---|---:|---:|---:|
| `.dram0.data` | 31 364 | 31 364 | +0 |
| `.dram1.data` | 0 | 0 | +0 |
| `.dram0.bss` | 45 196 | 45 220 | +24 |
| `.dram1.bss` | 18 148 | 16 668 | -1 480 |
| `.iram0.text` | 98 494 | 98 494 | +0 |
| `.tcm.text` | 794 | 794 | +0 |
| `.tcm.data` | 64 | 64 | +0 |
| `.rtc_noinit` | 1 200 | 1 200 | +0 |
| Total data+BSS internes | 94 708 | 93 252 | -1 456 |

Le gain global est **1 456 octets de data+BSS internes**, avec RTC et code IRAM inchangés. Ce n’est ni le tas libre après boot ni une mesure de fuite. La somme ne doit pas être additionnée à l’IRAM ou à la capacité physique des banques pour inventer un nouveau total RAM. Les 14 424 o de tables SHA1/XZ sont toujours en Flash, comme dans la référence ; aucun nouveau gain ne leur est attribué.

Le module de profilage passe de 578 o à 9 o de symboles BSS avant padding, avec 216 o RTC inchangés. Son état de 576 o PSRAM est créé à la première activation et conservé pour lecture/réutilisation. Ce gain nominal de module n’explique pas à lui seul le total du lien, qui comprend aussi les états simplifiés de l’interface et du bandeau.

## Comparaisons hôte réellement exécutées

| Scénario contrôlé | 0.8.42 | 0.8.43 | Portée |
|---|---:|---:|---|
| Accueil natif 120 s, ouvertures de fichiers | 5 333 | 0 | Vrais services Home/Catalogue, stockage simulé ; pas temps SD |
| Même scénario, appels d’énumération | 5 302 | 0 | Travail supprimé à l’accueil, pas dans une bibliothèque ouverte |
| Tables catalogue suivies, accueil sans ouverture de bibliothèque | 49 152 octets PSRAM | 24 576 o PSRAM | Allocation staging différée ; pas consommation totale du boot |
| Objets natifs du même scénario | 72 | 72 | Fonctionnalités d’accueil conservées |
| Flush LVGL au repos dans le scénario Home | 0 | 0 | Aucune preuve que l’ancien scan provoquait les flashs physiques |
| Copies String du lecteur d’indicateur audio sur 60 s, après initialisation | 1 440 | 0 | Vraies méthodes et adaptateurs de frontière, pas allocations P4 |
| Mutex demandés par ce lecteur | 240 | 0 | Producteur audio inchangé, 1 199 publications modèle |
| Test affichage inactif 180 s | 0 flush | 0 flush | Balayage matériel DSI non simulé par ce nombre |

Le scan utilise désormais une tâche ponctuelle cœur 0, priorité 1 et pile interne 8 Kio, plus ses objets RTOS. Cette allocation est **transitoire** ; elle ne doit pas être présentée comme un gain permanent ou comme une pile PSRAM. Elle passe par l’admission existante. En cas de refus de création, le comportement incrémental de secours préserve la fonctionnalité. Les deux cœurs P4 partagent les ressources ; le C6 conserve sa mémoire de communication indépendante.

## À relever sur Tab5

`m` et `tasks` à la demande : libre/minimum/plus grand bloc par HP, SPM/TCM, LP et PSRAM, piles actives, premier cycle puis 20 cycles identiques. Les minima ne remontent pas après une libération normale. Une grande PSRAM libre ne suffit pas à une allocation interne contiguë. Activer le profilage seulement pour le scénario, relever ensuite et le désactiver ; l’impression elle-même coûte du temps.

L’origine des flashs turquoise et la latence réelle restent ouvertes. Voir [TESTS_TAB5_0.8.43.md](TESTS_TAB5_0.8.43.md) et [AUDIT_0.8.43.md](AUDIT_0.8.43.md). Les commandes, empreintes et résultats sont joints dans `tools/verification`, avec les limites de chaque adaptateur hôte.
