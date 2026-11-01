# Vérification 0.8.33

Les correctifs de cette version sont contrôlés par les tests ci-dessous, avec des empreintes liant chaque rapport aux sources examinées. **Aucun essai sur un Tab5 physique n’a été effectué.**

## Firmware

La compilation complète a réussi avec Arduino CLI 1.5.1, M5Stack 3.3.9 et GCC RISC-V 14.2.0 (`esp-rv32/2601`), avec les dépendances épinglées de la 0.8.32. Cible : `m5stack:esp32:m5stack_tab5:ChipVariant=prev3,PSRAM=enabled,PartitionScheme=default,FlashMode=dio`.

Les 1 957 fichiers suivis du sketch sont restés inchangés pendant la compilation. Les 1 853 copies Arduino contrôlées correspondent aux sources ; les 382 lignes de prototypes générées sont identifiées et le corps du sketch est préservé. 1 403 objets ont été produits. L’ELF32 RISC-V et l’image ESP32-P4 passent les contrôles de format, checksum, hash et correspondance avec l’empreinte ELF embarquée. Les quatre verrous crypto contrôlés ont chacun une définition unique à l’origine attendue.

| Livrable / contrôle | Valeur |
| --- | --- |
| Image OTA | [Tab5OS_v0.8.33.bin](firmware/Tab5OS_v0.8.33.bin) |
| Octets BIN | 6019840 |
| Partition OTA | 6553600 octets |
| Marge | 533760 octets |
| SHA-256 | `ab5e9ece6bd993d0596a067feb494d6a67b7cf6acd349b2caa4240eca38197c2` |
| Preuve | [firmware-verification-0833.json](tools/verification/firmware-verification-0833.json) |

ELF et map associés dans `firmware/debug`. Pas d’image fusionnée ni de bootloader livré. Le choix prev3 n’établit pas à lui seul la compatibilité avec toutes les révisions matérielles.

## Contrôles exécutés sur les sources 0.8.33

| Contrôle | Résultat et portée | Rapport |
| --- | --- | --- |
| Gestionnaire mémoire | PASS — 2 033 contrôles et 400 cycles : admission, domaines SRAM/PSRAM, fragmentation, récupération et libération ; tas et RTOS simulés. | [memory-0833.json](tools/verification/memory-0833.json) |
| Régression mémoire | PASS — Cinq défauts reproduits sur les sources exactes livrées en 0.8.32 et absents du chemin corrigé ; 12 contrôles. | [memory-regression-0833.json](tools/verification/memory-regression-0833.json) |
| Intégration mémoire | PASS — 40 contrôles sur les corps extraits du sketch : 11 cas de premier plan et neuf rappels de caches. | [memory-integration-0833.json](tools/verification/memory-integration-0833.json) |
| Présentation écran | PASS — 9 578 contrôles du pilote de production inchangé : trames partielles, événements de synchronisation, échecs et reprise ; SDK/IRQ/cache simulés. | [display-0833.json](tools/verification/display-0833.json) |
| Moteur Git réel | PASS — 38 scénarios : 25 avec un serveur Git HTTPS local et 13 sur disque, dont branches de suivi, refus serveur et fichiers modifiés sans changement de taille/date. | [git-engine-0833.json](tools/verification/git-engine-0833.json) |
| Explorateur / Git / PIN | PASS — 521 contrôles, dont 100 cycles de dialogues ; vrais menus, GitView, dialogue PIN et LVGL, backend/crypto/RTOS simulés. | [explorer-git-0833.json](tools/verification/explorer-git-0833.json) |
| Atelier Image natif | PASS — 245 contrôles, huit captures et cinq fermetures : vraie LVGL, police OS, focus, saisie, outils, sauvegarde, suspension et erreurs d’allocation ; frontières matérielles simulées. | [image-native-0833.json](tools/verification/image-native-0833.json) |
| Moteur image | PASS — 34 342 contrôles, 400 cycles et 336 scénarios d’allocation : pixels, codecs, calques, historique et échecs. | [image-core-0833.json](tools/verification/image-core-0833.json) |
| Rendu image régional | PASS — 1 125 671 contrôles : résultat identique des pinceaux optimisés, limites et découpages de rectangles, cas à un pixel et débordements. | [image-render-0833.json](tools/verification/image-render-0833.json) |

Les purges après inactivité, les collectes périodiques sous marge, l’éviction du premier plan ou du demandeur et la collecte différée provoquée par un ancien échec d’allocation sont reproduites dans le banc de régression mémoire. Elles ne se produisent plus sur le chemin corrigé. Les diagnostics d’allocation sont conservés. Les marges de sécurité restent inchangées ; voir [MEMOIRE_0833.md](MEMOIRE_0833.md).

Le banc Git compile les 216 unités C libgit2 livrées et le code adaptateur de production. Le serveur HTTPS réel est local à l’ordinateur ; le transport utilise une adaptation OpenSSL hôte. Ce test ne remplace pas le chemin mbedTLS, la radio C6 ou le système FAT de la tablette. Les modifications à taille/date inchangées sont reproduites sur disque en imposant les métadonnées correspondantes. Voir [GIT.md](GIT.md) et les preuves HTTPS/local liées au rapport.

Le banc Atelier Image exécute le véritable éditeur natif, LVGL, les gestionnaires de focus/saisie et le service d’écriture. L’admission mémoire et les frontières matérielles sont simulées. Le document et son historique survivent à la navigation ; l’aperçu dérivé est libéré à cette étape. Les échecs d’allocation d’aperçus très étroits sont testés. Le compteur de PSRAM suivi revient à zéro après fermeture ou destruction du parent. Les captures sont sous [image-native-ui-0833](tools/verification/image-native-ui-0833). Le texte inséré dans les pixels conserve sa police bitmap ; les contrôles de l’application utilisent la police de l’OS.

ASan et UBSan sont utilisés par les bancs qui les indiquent. LeakSanitizer est désactivé dans cet environnement ; les compteurs d’allocation explicitement instrumentés sont vérifiés. Les tests de sauvegarde couvrent les échecs traités par le code, sans prétendre garantir l’atomicité d’une carte FAT lors d’une coupure électrique.

## Ressources et paquet

175 ressources Factory et 40 ressources de secours embarquées sont vérifiées, ainsi que la syntaxe des modules Python. Les 14 packs de langue couvrent les 3 303 clés de référence, sans entrée manquante ni erreur de paramètres, de retours à la ligne ou de glyphes dans les contrôles effectués. Les nouveaux contrôles natifs d’Atelier Image sont traduits dans ces packs.

Preuves : [resources-0833.json](tools/verification/resources-0833.json), [languages-0833.json](tools/verification/languages-0833.json). L’ancien ELF Atelier Image et son profil de lancement ne sont plus distribués. Le paquet doit passer les contrôles avant création et après extraction ZIP : sources compilées, rapports, BIN/ELF/map, ressources SD/Factory et inventaire de publication.

## Validation matérielle restante

Les contrôles hôte ne prouvent ni l’origine ni la disparition des flashs physiques, et ne mesurent pas la fluidité du Tab5. Le pilote d’affichage reste inchangé. Les séquences partielles et reprises passent sur hôte, sans rafraîchissement provoqué par le temps seul.

Après installation conjointe du firmware et des ressources SD, les essais prioritaires sont : naviguer entre applications et observer l’affichage/mémoire ; dessiner, annuler et reprendre Atelier Image ; modifier puis pousser un dépôt réel, récupérer une modification distante et vérifier les fichiers ; répéter les recherches Cartes et les dialogues PIN avec Entrée/Échap. Ces essais restent à effectuer sur la tablette.

[VERIFICATION_0.8.32.md](VERIFICATION_0.8.32.md) conserve les résultats antérieurs. Les autres rapports 0.8.31/0.8.32 présents sont historiques et ne remplacent pas les preuves de cette version. [NOUVEAUTES_v0.8.33.md](NOUVEAUTES_v0.8.33.md) décrit les changements et limites fonctionnelles.

Les anciennes sorties intermédiaires de rendu hôte sous `tools/native-apps/image-editor/test-output` sont exclues de l’archive. Les sources et les captures finales vérifiées sont conservées.
