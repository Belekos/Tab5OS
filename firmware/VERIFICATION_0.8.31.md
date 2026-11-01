# Vérification 0.8.31

<!-- build-status -->
**Compilation complète 0.8.31 et contrôles du firmware réussis sous Linux/GCC.** Code de sortie : 0. Chaîne : Arduino CLI 1.5.1, core M5Stack 3.3.9 et GCC Espressif RISC-V 14.2.0 (`esp-rv32/2601`). Preuve : [firmware-verification-0831.json](tools/verification/firmware-verification-0831.json).

Les 1 936 fichiers sources contrôlés sont restés inchangés pendant la compilation, qui a produit 1 400 fichiers objets. Les copies Arduino sont conformes ; le sketch généré conserve le source et ajoute les prototypes attendus. L’ELF est RISC-V 32 bits, avec ABI flottante simple précision. L’image ESP32-P4 passe les contrôles de checksum et de hash de validation ; son hash ELF embarqué correspond à l’ELF compilé. Le rapport vérifie aussi l’origine et l’unicité des symboles du correctif de verrouillage crypto.

| Contrôle | Valeur vérifiée |
| --- | --- |
| Image OTA livrée | [firmware/Tab5OS_v0.8.31.bin](firmware/Tab5OS_v0.8.31.bin) |
| Taille de l’image | 5 947 808 octets |
| Partition OTA | 6 553 600 octets |
| Marge dans la partition | 605 792 octets |
| SHA-256 du BIN | `ef6e04ea8affc01543639629376a4692cd870be829463584a110d41309343c0f` |
| Cible de compilation | Tab5 ESP32-P4 `prev3`, PSRAM activée, partitions par défaut, flash DIO |

Les fichiers ELF et map correspondants sont livrés dans `firmware/debug`. L’image fusionnée, le chargeur de démarrage et le fichier de partitions produits par Arduino ne sont pas distribués. Les détails d’en-tête figurent dans [image-info.log](tools/verification/build-0831/image-info.log). La cible `prev3` et une plage de révision déclarée dans un en-tête ne démontrent pas la compatibilité avec toutes les générations matérielles de Tab5. Aucun flash ni essai sur appareil physique n’est revendiqué.
<!-- /build-status -->

<!-- test-results -->
Les contrôles ci-dessous portent sur les sources 0.8.31. Chaque rapport conserve les empreintes des sources et bancs d’essai concernés. Les tests hôtes restent distincts de la compilation complète et des essais sur tablette.

| Contrôle | Résultat et portée | Rapport |
| --- | --- | --- |
| Mémoire de l’OS | **PASS** : 1 640 vérifications, 400 cycles ; gestionnaire de production, tas et callbacks contrôlés, ASan/UBSan. | [memory-0831.json](tools/verification/memory-0831.json) |
| Admission et allocations Git | **PASS** : 7 804 vérifications, 400 sessions d’allocation ; allocateur, budget, passage des réservations et logique du worker d’interface de production, tas simulé, ASan/UBSan. | [git-0831.json](tools/verification/git-0831.json) |
| Cartes, saisie et clavier | **PASS** : 29 138 vérifications avec LVGL 9.6, TinyTTF et ArduinoJson 7.4.3 réels ; contrôleurs de production et worker avec transferts scriptés, ASan/UBSan. | [maps-0831.json](tools/verification/maps-0831.json) |
| Cycle de vie de la tâche audio | **PASS** : 400 cycles répétés ; logique de production et trampoline des tâches, threads hôtes et services simulés, ASan/UBSan. | [audio-worker-0831.json](tools/verification/audio-worker-0831.json) |
| Wi-Fi et compatibilité C6 | **PASS** : 45 862 vérifications ; adaptateur/politique de production et méthodes Wi-Fi extraites, interfaces SDK/réseau simulées, ASan/UBSan. | [compatibility-0831.json](tools/verification/compatibility-0831.json) |
| Autosauvegarde et batterie | **PASS** : 3 362 vérifications ; méthodes de l’éditeur extraites, interface et protection batterie de production, stockage/capteurs simulés, UBSan. | [autosave-battery-0831.json](tools/verification/autosave-battery-0831.json) |
| Ressources | **PASS** : 174 ressources Factory, 40 ressources de secours firmware, correspondance avec la SD et syntaxe Python. | [resources-0831.json](tools/verification/resources-0831.json) |
| Langues | **PASS** : les 14 packs couvrent les 3 178 clés de référence ; aucune clé ajoutée dans cette version. | [languages-0831.json](tools/verification/languages-0831.json) |

### Mémoire et Git

Le gestionnaire de production est testé sur le cas exact de 46 Kio disponibles pour 48 Kio requis, avec une libération simulée de pile de 16 Kio. Le banc contrôle la sélection du bon domaine mémoire, les réservations concurrentes, les blocs contigus/DMA, le veto d’un utilisateur actif, les demandes issues d’une tâche de fond, les délais d’attente et les libérations différées. L’expiration des caches teste aussi le retour au premier plan, la limite d’un rappel par seconde et le débordement du compteur de temps. Les callbacks représentent des ressources contrôlées ; l’enregistrement des callbacks des applications est relié à l’empreinte du code d’intégration, sans exécution de toute l’interface.

Le banc Git simule le budget disponible avant et après la pile de 40 Kio, puis vérifie que la réservation de session est transmise sans nouvelle admission ni double charge. Il couvre l’échec avant création, le nettoyage, le maintien des marges, la fragmentation, les lots PSRAM, la limite d’allocation/réallocation et les échecs de **Cloner/Nouveau dépôt** sans `git status` secondaire. Il ne réalise aucun clone réseau, opération libgit2 de transport, échange TLS ou ordonnancement FreeRTOS réel.

### Recherches successives et durée de vie de la saisie

Le chemin de saisie 0.8.30 reproduit réellement un accès à une zone libérée sous AddressSanitizer : après validation et destruction du champ de recherche, l’entretien de la saisie conservait son adresse. Avec le correctif, 400 cycles de recherche latine et 300 cycles de changement/suppression de saisie CJK passent ; un échec d’installation de l’écouteur de destruction désactive proprement le suivi. Ces essais comptent 12 707 vérifications.

Le worker Cartes de production est aussi exercé sur 400 recherches, 40 annulations actives avec remplacement et 40 arrêts/redémarrages. Des files et transferts hôtes scriptés contrôlent la propriété des résultats : au plus un tampon possédé et aucun à la fin. Ces 4 203 vérifications ne passent pas par le réseau réel. Les 12 228 vérifications restantes couvrent le parseur, ses échecs d’allocation et les parcours clavier corrigés en 0.8.30, dont la validation par Entrée/Espace.

Ces tests utilisent les allocations de l’ordinateur à la place de la PSRAM. Aucun fournisseur de lieux, lien HTTP/TLS/C6 ou support SD physique n’est sollicité. Ils établissent le défaut de durée de vie de la saisie, sans établir que cet unique défaut explique exactement le plantage observé sur la tablette.

### Audio et fonctions conservées

Le banc audio vérifie le démarrage à la demande, la fin coopérative, les courses au démarrage/à l’arrêt, les commandes arrivant en fin de tâche, la reprise d’une session prioritaire et la récupération différée de la pile. Les compteurs de pile et de réservations retombent à zéro après les cycles. Les tâches, pilotes, décodeurs et opérations réseau sont simulés ; les 16 Kio de pile libérables sont une taille déclarée dans le code et contrôlée par le banc, pas une mesure du gain de SRAM sur un Tab5.

Les tests Wi-Fi vérifient notamment que le numéro de version C6 ne bloque pas les opérations ; ils ne démontrent aucune connexion radio réelle ni compatibilité universelle des firmwares C6. Les essais batterie vérifient l’absence de sauvegarde forcée, sans mesurer la recharge ou l’arrêt physique d’une tablette.

LeakSanitizer est désactivé en raison des limites de l’environnement ; AddressSanitizer et UndefinedBehaviorSanitizer restent actifs dans les bancs indiqués. Les vérifications de langues conservent les différences de séparation en paragraphes signalées et ne constituent pas un contrôle visuel.
<!-- /test-results -->

## Périmètre et preuves antérieures

La 0.8.31 corrige la référence de saisie conservée après suppression, prépare le besoin complet de Git avant sa tâche et renforce le gestionnaire central existant : choix du domaine mémoire, expiration des ressources rechargeables en arrière-plan et cycle de vie de la tâche audio. Les marges de sécurité restent inchangées. Voir [NOUVEAUTES_v0.8.31.md](NOUVEAUTES_v0.8.31.md).

[VERIFICATION_0.8.30.md](VERIFICATION_0.8.30.md) conserve le rapport précédent. Ses références et compteurs ne décrivent pas cette livraison. Aucun résultat d’une ancienne version n’est attribué automatiquement aux sources modifiées.

## Essais sur tablette

Aucun flash ni essai sur Tab5 physique n’est revendiqué pour la 0.8.31. Les essais suivants restent à effectuer avec le firmware livré :

- Effectuer plusieurs recherches Cartes successives, avec sélection de résultat, nouvelle recherche, annulation et retour dans l’application.
- Cloner le dépôt précédemment refusé dans un nouveau dossier, puis relever les mesures mémoire et le message exact en cas de refus.
- Alterner Cartes, Explorateur/Git et les autres applications ; vérifier aussi la lecture audio, le Wi-Fi et l’enregistrement après une période inactive.
- Relever la mémoire libre et les plus grands blocs avant/après ces opérations. En cas de redémarrage, relever **Dernier reset** et **Avant le reset (contexte)** avant de relancer l’action.

La disparition des incidents sur le matériel ne peut être déduite de la seule compilation ou des tests hôtes.
