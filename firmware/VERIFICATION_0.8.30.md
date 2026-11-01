# Vérification 0.8.30

<!-- build-status -->
**Compilation complète 0.8.30 et contrôles du firmware réussis sous Linux/GCC.** Le code de sortie de la compilation est 0. La chaîne utilisée est Arduino CLI 1.5.1, core M5Stack 3.3.9 et GCC Espressif RISC-V 14.2.0 (`esp-rv32/2601`). Preuve : [firmware-verification-0830.json](tools/verification/firmware-verification-0830.json).

Les 1 935 fichiers sources contrôlés sont restés inchangés pendant la compilation, qui a produit 1 400 fichiers objets. Les copies Arduino ont été vérifiées ; le sketch généré conserve le source et ajoute les déclarations de prototypes attendues. L’ELF est RISC-V 32 bits, avec ABI flottante simple précision. L’image ESP32-P4 passe les contrôles de checksum et de hash de validation ; son hash ELF embarqué correspond à l’ELF compilé. La provenance des symboles de verrouillage ECDSA/SHA-AES dans l’objet de compatibilité du projet est également vérifiée.

| Contrôle | Valeur vérifiée |
| --- | --- |
| Image OTA livrée | [firmware/Tab5OS_v0.8.30.bin](firmware/Tab5OS_v0.8.30.bin) |
| Taille de l’image | 5 943 568 octets |
| Partition OTA | 6 553 600 octets |
| Marge dans la partition | 610 032 octets |
| SHA-256 du BIN | `bbaf9b24c4bdbd821af278bc0606121a49e067a91981dae124205074e85750bc` |
| Cible de compilation | Tab5 ESP32-P4 `prev3`, PSRAM activée, partitions par défaut, flash DIO |

Les fichiers ELF et map correspondants sont livrés dans `firmware/debug`. Le rapport technique recense aussi des fichiers intermédiaires générés par Arduino ; l’image fusionnée et le chargeur de démarrage ne sont pas distribués. Les détails de l’en-tête figurent dans [image-info.log](tools/verification/build-0830/image-info.log). Sa plage de révision silicium déclarée, 0.0 à 655.35, ne démontre pas une compatibilité avec toutes les générations de Tab5 : la cible de compilation reste `prev3`. Aucun flash ni essai sur appareil physique n’est revendiqué.
<!-- /build-status -->

<!-- test-results -->
Les contrôles ci-dessous portent sur la 0.8.30. Les rapports conservent les empreintes des sources et bancs d’essai concernés ; ils restent distincts de la compilation complète et des essais sur tablette.

| Contrôle | Résultat et portée | Rapport |
| --- | --- | --- |
| Admission mémoire Git | **PASS** : allocateur de production et budget de lancement extraits à l’identique, `AllocationBudget` et gestionnaire de ressources réels, ASan/UBSan. 7 664 vérifications, dont 400 sessions répétées. Tas SRAM/PSRAM et fragmentation simulés, tampons hôtes réels. | [git-0830.json](tools/verification/git-0830.json) |
| Navigation clavier et Cartes | **PASS** : LVGL 9.6, TinyTTF et ArduinoJson 7.4.3 réels, contrôleurs de résultats/encodeur extraits et classes de production du focus et de la saisie. 12 228 vérifications sous ASan/UBSan. Quatre parcours 0.8.29 reproduisent un accès libéré ; les versions corrigées passent. | [maps-0830.json](tools/verification/maps-0830.json) |
| Wi-Fi et compatibilité C6 | **PASS** : adaptateur/politique de production et trois méthodes Wi-Fi extraites, ASan/UBSan, absence du symbole RPC de version vérifiée. Interfaces SDK et réseau simulées. | [compatibility-0830.json](tools/verification/compatibility-0830.json) |
| Autosauvegarde et batterie | **PASS** : 3 362 vérifications sous UBSan ; méthodes de production de l’éditeur extraites, code d’interface batterie inclus et protection batterie réelle, avec doubles de stockage, d’interface et de capteurs. | [autosave-battery-0830.json](tools/verification/autosave-battery-0830.json) |
| Ressources | **PASS** : 174 ressources Factory, 40 ressources du secours firmware, correspondance des données embarquées avec la SD et syntaxe Python. | [resources-0830.json](tools/verification/resources-0830.json) |
| Langues | **PASS** : les 14 packs couvrent les 3 178 clés de référence, dont le nouveau diagnostic Git. | [languages-0830.json](tools/verification/languages-0830.json) |

Les essais Git reproduisent le refus de l’ancien budget fixe de 8 Mio avec 4 Mio libres, puis l’admission du petit ensemble de données du test. Ils couvrent la réservation par tranches et son repli à la taille exacte, les réserves concurrentes, les limites d’allocation/réallocation, les échecs, l’annulation et la libération après des sessions répétées. Ils ne sollicitent pas le transport libgit2, le vrai TLS, l’ordonnancement FreeRTOS ou un clone sur appareil. Ils ne prouvent pas qu’un dépôt arbitraire tiendra en mémoire.

Les essais Cartes couvrent dix noms à plusieurs mots, la sélection de résultats par Entrée et Espace sur 200 cycles chacune, la validation de champs monolignes détruits ou remplacés, et des contrôles détruits pendant leur prise de focus initiale. Les quatre versions 0.8.29 correspondantes échouent réellement sous AddressSanitizer ; leurs traces sont conservées dans le rapport. Le parseur existant est également contrôlé sur 300 cycles et 24 points d’échec d’allocation. Ces essais utilisent les allocations de l’ordinateur à la place de la PSRAM ; aucune requête réelle au fournisseur de lieux, liaison HTTP/TLS/C6 ou carte SD physique n’est testée. Les espaces s’encodent déjà en `%20` : le lien causal entre deux mots et l’incident précis sur tablette reste non établi.

Les essais Wi-Fi ne démontrent pas une connexion radio réelle ni la compatibilité universelle des firmwares C6. Les essais batterie ne mesurent pas l’arrêt ou la recharge d’une tablette. LeakSanitizer est désactivé dans les bancs ASan en raison des limites de l’environnement ; AddressSanitizer et UndefinedBehaviorSanitizer restent actifs. Le rapport de langues conserve les différences de séparation en paragraphes signalées et ne constitue pas une vérification visuelle.

Aucun résultat des versions précédentes n’est automatiquement considéré valide pour une source modifiée.
<!-- /test-results -->

## Périmètre

- Remplacement de la réservation Git préalable de 8 Mio par une admission progressive, en conservant le plafond et les réserves du système.
- Correction d’accès à des objets LVGL détruits pendant une validation clavier de résultat ou de champ, ainsi que pendant leur prise de focus initiale ; le nombre de mots n’est pas établi comme cause.
- Conservation des corrections Wi-Fi et autosauvegarde de la 0.8.29.

Les défauts reproduits, les changements et leurs limites sont détaillés dans [NOUVEAUTES_v0.8.30.md](NOUVEAUTES_v0.8.30.md). La compilation complète et les contrôles du binaire constituent une étape distincte. Le retour matériel ne permet pas encore d’affirmer que ces défauts sont les seules causes des incidents signalés.

## Preuves antérieures

[VERIFICATION_0.8.29.md](VERIFICATION_0.8.29.md) conserve le rapport de la version précédente. Ses références au binaire 0.8.29 et ses compteurs ne décrivent pas la livraison 0.8.30. Les rapports plus anciens restent également historiques. Leur réutilisation demande de vérifier le périmètre et l’identité des sources et du banc d’essai concernés.

## Essais sur tablette

Aucun flash ni essai sur Tab5 physique n’est revendiqué pour la 0.8.30. Les essais suivants restent à effectuer avec le firmware livré :

- Cloner le dépôt qui était refusé dans un nouveau dossier, en relevant le message exact et les mesures mémoire si un refus persiste.
- Relancer la recherche de lieu qui provoquait le plantage, puis essayer une recherche à un mot et une autre à plusieurs mots.
- En cas de redémarrage, relever **Dernier reset** et **Avant le reset (contexte)** avant de relancer l’action.

La disparition de ces défauts sur le matériel ne peut être déduite de la seule compilation.
