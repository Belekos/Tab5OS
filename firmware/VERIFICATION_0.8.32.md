# Vérification 0.8.32

La compilation complète du firmware et les tests indiqués ci-dessous ont réussi. Les rapports lient les résultats aux empreintes des fichiers concernés. **Aucun essai sur un Tab5 physique n’a été effectué.**

## Firmware

Arduino CLI 1.5.1, M5Stack 3.3.9, GCC RISC-V 14.2.0 (`esp-rv32/2601`), bibliothèques épinglées de la 0.8.31. Cible ESP32-P4 prev3, PSRAM, partitions par défaut, DIO. Les 1 962 fichiers du sketch surveillés sont restés inchangés durant la compilation ; 1 401 objets ont été produits. Le sketch Arduino généré conserve les sources. ELF 32 RISC-V et image ESP32-P4 passent les contrôles de format, checksum, hash et correspondance avec le hash ELF embarqué. Les verrous crypto conservent une définition unique avec l’origine attendue.

| Livrable / contrôle | Valeur |
| --- | --- |
| Image OTA | [Tab5OS_v0.8.32.bin](firmware/Tab5OS_v0.8.32.bin) |
| Octets BIN | 5967488 |
| Partition OTA | 6553600 |
| Marge | 586112 octets |
| SHA-256 | `5d88c5df8019203effc4d9fb7f4f1f738713697258adb75bc1b6393c67189673` |
| Preuve | [firmware-verification-0832.json](tools/verification/firmware-verification-0832.json) |

ELF et map associés dans `firmware/debug`. Pas d’image fusionnée ni de bootloader livré. Le choix prev3 n’établit pas à lui seul la compatibilité avec toutes les révisions matérielles.

## Tests actuels

| Contrôle | Portée | Rapport |
| --- | --- | --- |
| Git réel local | PASS — Production libgit2, dépôts réels sur disque, index/commit/config/diff vérifiés par Git de bureau ; transport HTTPS exclu. | [git-engine-0832.json](tools/verification/git-engine-0832.json) |
| Admission mémoire Git | PASS — Budget de démarrage, worker, allocations et libérations ; tas/RTOS simulés. | [git-memory-0832.json](tools/verification/git-memory-0832.json) |
| Explorateur / Git / PIN | PASS — GitView, menus et vrai dialogue PIN avec LVGL ; backend/crypto simulés. | [explorer-git-0832.json](tools/verification/explorer-git-0832.json) |
| Coffre / clavier | PASS — Cycles de saisie, annulation, focus et formulaires ; crypto/RTOS simulés. | [vault-keyboard-0832.json](tools/verification/vault-keyboard-0832.json) |
| Moteur image | PASS — Opérations de pixels, calques, historique, codecs, erreurs et libérations. | [image-core-0832.json](tools/verification/image-core-0832.json) |
| Éditeur ELF | PASS — Interface réelle exécutée sur hôte, rendu et interactions ; ELF P4 compilé. | [image-editor-0832.json](tools/verification/image-editor-0832.json) |
| Import BMP32 | PASS — Orientations, octet réservé, troncatures et en-têtes invalides ; image précédente préservée. | [image-editor-bmp32-0832.json](tools/verification/image-editor-bmp32-0832.json) |
| Services ABI6 | PASS — File bornée et écriture progressive de production ; échecs du système de fichiers simulés. | [abi6-0832.json](tools/verification/abi6-0832.json) |
| Cartes / IME | PASS — LVGL/TinyTTF/ArduinoJson réels, recherches et annulations répétées ; réseau scripté. | [maps-0832.json](tools/verification/maps-0832.json) |
| Mémoire OS | PASS — Gestionnaire de production, admission/fragmentation/caches ; tas simulé. | [memory-0832.json](tools/verification/memory-0832.json) |
| Tâche audio | PASS — 400 cycles de cycle de vie ; pilotes et tâches simulés, compteurs finaux à zéro. | [audio-worker-0832.json](tools/verification/audio-worker-0832.json) |
| Compatibilité C6 | PASS — Politique/adaptateur de production ; SDK et radio simulés. | [compatibility-0832.json](tools/verification/compatibility-0832.json) |
| Autosave / batterie | PASS — Éditeur et protection batterie avec stockage/capteurs simulés. | [autosave-battery-0832.json](tools/verification/autosave-battery-0832.json) |

ASan et UBSan sont utilisés par les bancs indiqués dans leurs rapports ; le banc batterie utilise UBSan. LeakSanitizer est désactivé à cause des restrictions de l’environnement. Les compteurs d’allocation explicitement testés retombent à zéro. Les rapports d’examen indépendant sont distincts des tests exécutés.

Le test Git compile les 216 unités C libgit2 livrées et le code adaptateur de production, sur un système 64 bits hôte avec transport HTTPS désactivé. Il reproduit aussi les anciens défauts de portée de `add .` et d’ajout explicite de racine. Il ne démontre pas un échange avec un serveur Git sur Tab5. Les contrôles d’interface utilisent le backend simulé, tandis que le moteur Git fait l’objet du banc réel séparé.

Le chemin IME 0.8.30 conserve son échec ASan de référence ; le chemin corrigé passe les cycles répétés de recherche/suppression. Ce défaut logiciel reproduit ne démontre pas à lui seul la cause exacte de chaque plantage observé sur appareil.

Les captures de l’éditeur viennent de son véritable moteur de rendu exécuté sur ordinateur. Les tests ABI 6 exercent pertes d’événements, saturation, écritures courtes, erreurs de fermeture/renommage et récupération. Ils ne simulent pas toute la physique d’une carte FAT coupée pendant une écriture. Les mutations et buffers sont nettoyés après arrêt du worker ; aucun gain de SRAM physique n’est mesuré.

## Ressources

177 ressources Factory, 40 ressources de secours embarquées et syntaxe Python vérifiées. Les 14 packs de langue couvrent les 3196 clés de référence, sans erreur de format ni glyphe manquant dans les polices contrôlées. Atelier Image possède sa propre interface français/anglais et sa police bitmap ; ce contrôle des 14 packs concerne l’OS.

Preuves : [resources-0832.json](tools/verification/resources-0832.json), [languages-0832.json](tools/verification/languages-0832.json). Le paquet est contrôlé avant et après extraction ZIP : sources compilées, rapports, BIN/ELF/map, application ELF, ressources SD/Factory et inventaire de publication.

## Validation sur tablette à poursuivre

Installer ensemble firmware et ressources SD, puis essayer Git depuis la racine, un sous-dossier et un fichier ; commit, clone/fetch/pull/push avec origin et identifiants de test. Vérifier PIN faux/correct/annulé et Entrée/Échap avec clavier USB et tactile. Répéter deux recherches Cartes, ouvrir/fermer l’éditeur, dessiner/annuler, enregistrer/réouvrir T5I/BMP et surveiller SRAM/PSRAM. Ces essais restent à faire sur matériel.

[VERIFICATION_0.8.31.md](VERIFICATION_0.8.31.md) conserve les résultats historiques ; ils ne remplacent pas les preuves de cette version. [NOUVEAUTES_v0.8.32.md](NOUVEAUTES_v0.8.32.md) décrit les fonctions et limites.

Les sorties intermédiaires de rendu hôte sous `tools/native-apps/image-editor/test-output` sont exclues de l’archive ; les captures vérifiées sont conservées sous `tools/verification/image-editor-ui-0832`. Cette exclusion ne concerne aucune source du firmware, de l’application ou de ses tests.
