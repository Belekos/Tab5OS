# Bilan des ressources — Tab5OS 0.8.42

Les valeurs Flash et sections proviennent du BIN/ELF final réellement compilé, comparé à l’archive livrée 0.8.41. Les mesures hôte ci-dessous décrivent leurs scénarios, pas la tablette. Aucun total de tas libre après démarrage, temps de boot, fragmentation réelle, CPU ou consommation électrique n’est déduit des sections de lien.

## Flash et OTA

| Mesure | Référence 0.8.41 | 0.8.42 vérifiée |
|---|---:|---:|
| Firmware applicatif | 6 310 368 octets | 6 060 672 octets |
| Capacité de chaque partition OTA | 6 553 600 octets | 6 553 600 octets |
| Marge disponible | 243 232 octets | 492 928 octets |
| Occupation du slot | 96.29 % | 92.48 % |

Gain net du BIN : **249 696 octets**. Table des partitions strictement identique : NVS, otadata, deux slots OTA de 6,25 Mio, SPIFFS et coredump. Pas de migration, chiffrement ou formatage entrepris. L’indicateur Application/OTA compare de la Flash : la nouvelle image occupe 5.780 Mio sur 6,25 Mio, sans rapport avec le tas SRAM.

Les 40 ressources finales de réparation représentent 378 724 octets reconstruits exactement et 113 783 octets compressés. Leur gain théorique de données est 264 941 octets. Le net du BIN diffère : nouveau manifeste de version, décompression, diagnostics et autres corrections ajoutent du code/données. Les fichiers complets restent distribués sur SD ; aucune application ni police n’a été supprimée.

## Sections mémoire liées

| Section | 0.8.41, octets | 0.8.42, octets | Variation nouveau − ancien |
|---|---:|---:|---:|
| `.dram0.data` | 31 364 | 31 364 | +0 |
| `.dram1.data` | 0 | 0 | +0 |
| `.dram0.bss` | 44 692 | 45 196 | +504 |
| `.dram1.bss` | 17 884 | 18 148 | +264 |
| `.iram0.text` | 98 494 | 98 494 | +0 |
| `.tcm.text` | 794 | 794 | +0 |
| `.tcm.data` | 64 | 64 | +0 |
| `.rtc_noinit` | 984 | 1 200 | +216 |
| Somme data+BSS internes ci-dessus | 93 940 | 94 708 | +768 |

La somme data+BSS augmente de 768 octets si le nombre est positif ; cette variation comprend les nouveaux états de synchronisation/diagnostic. La région RTC est séparée. Les 14 424 octets des tables SHA1/XZ restent en Flash comme en 0.8.41 : aucun nouveau gain RAM ne leur est attribué. IRAM est du code interne ; SPM/TCM/LP ne sont pas de la PSRAM interchangeable. Le placement et l’agrégat INTERNAL sont décrits dans l’audit mémoire.

## Comparaisons effectivement exécutées sur hôte

| Scénario contrôlé | Avant | Après | Limite |
|---|---:|---:|---|
| Affichage : attente maximale handler, DMA à 16 ms | 24 ms | 0 ms | Modèle déterministe LVGL réel, pas latence P4 |
| Même modèle, intervalle maximal du service d’entrée | 32 ms | 8 ms | Boucle modèle à 8 ms |
| DMA suspendu, attente maximale handler | 100 ms | 0 ms | La publication reste différée tant que le buffer est occupé |
| Rapports de survol souris distribués pour un lot de 32 | 32 | 1 | Les 20 points d’un tracé avec bouton restent 20 |
| Parcours de tas sur pénurie interne stable de 20 ms | 66 | 4 | Comptage hôte, essai dépend de l’ordonnanceur hôte |
| Copies de chaînes pour les 16 accesseurs audio scalaires, 10 000 itérations | 960 000 | 0 | Compteur de copies du type String hôte, pas allocation physique |
| Vue native, attente UI sous contention artificielle de 80 ms | ~80,7 ms | ~0,002 ms | Test contention hôte, pas benchmark ESP32 |
| Réparation, pic PSRAM suivi par adaptateur hôte | Sans décompression | 95 627 octets | Seulement pendant la réparation explicite ; zéro allocation au repos |

Le buffer de réparation et le workspace zlib sont libérés à chaque fin/échec/annulation. La sortie est bornée à 128 Kio +1 et le workspace à 48 Kio, sans repli SRAM. Les tests couvrent les échecs d’allocation, flux invalides et fautes d’E/S. Aucun nouveau buffer d’image plein écran n’est ajouté au pipeline DSI.

## Mesures à faire sur appareil

Libre/minimum/plus grand bloc par HP, SPM/TCM, LP et PSRAM ; marges de piles et tâches actives ; démarrage froid avec même SD/options ; vingt cycles d’ouverture/fermeture ; radio/vidéo/cartes simultanées ; latences UI et compteurs DSI pendant les flashs. Le minimum depuis boot peut rester bas après une libération normale. Une grande PSRAM libre ne suffit pas à une allocation interne ou DMA contiguë.

Voir [TESTS_TAB5_0.8.42.md](TESTS_TAB5_0.8.42.md). Les diagnostics détaillés sont à la demande ; ils peuvent eux-mêmes retarder brièvement la tâche et ne doivent pas tourner en continu pendant une mesure de fluidité.
