# Vérification 0.8.29

<!-- build-status -->
**Compilation complète 0.8.29 et contrôles du firmware réussis sous Linux/GCC.** Le code de sortie de la compilation est 0. La chaîne utilisée est Arduino CLI 1.5.1, core M5Stack 3.3.9 et GCC Espressif RISC-V 14.2.0 (`esp-rv32/2601`). Preuve : [firmware-verification-0829.json](tools/verification/firmware-verification-0829.json).

Les 1 934 fichiers sources contrôlés sont restés inchangés pendant la compilation, qui a produit 1 400 fichiers objets. Les copies Arduino ont été vérifiées ; le sketch généré conserve le source et ajoute les déclarations de prototypes attendues. L’ELF est RISC-V 32 bits, avec ABI flottante simple précision. L’image ESP32-P4 passe les contrôles de checksum et de hash de validation ; son hash ELF embarqué correspond à l’ELF compilé. La provenance des symboles de verrouillage ECDSA/SHA-AES dans l’objet de compatibilité du projet est également vérifiée.

| Contrôle | Valeur vérifiée |
| --- | --- |
| Image OTA livrée | [firmware/Tab5OS_v0.8.29.bin](firmware/Tab5OS_v0.8.29.bin) |
| Taille de l’image | 5 941 248 octets |
| Partition OTA | 6 553 600 octets |
| Marge dans la partition | 612 352 octets |
| SHA-256 du BIN | `535ecc607f0b08e791d50921fc2a7dfd77c1baedba22fbc124a90d16e3345aa3` |
| Cible de compilation | Tab5 ESP32-P4 `prev3`, PSRAM activée, partitions par défaut, flash DIO |

Les fichiers ELF et map correspondants sont livrés dans `firmware/debug`. Le rapport technique recense aussi des fichiers intermédiaires générés par Arduino ; l’image fusionnée et le chargeur de démarrage ne sont pas distribués. Les détails de l’en-tête figurent dans [image-info.log](tools/verification/build-0829/image-info.log). Les bornes larges de révision contenues dans cet en-tête ne démontrent pas une compatibilité avec toutes les générations de Tab5. Aucun flash ni essai sur appareil physique n’est revendiqué.
<!-- /build-status -->

<!-- test-results -->
Les contrôles suivants ont été exécutés pour les sources et ressources 0.8.29. Ils restent distincts d’une compilation complète du firmware et d’un essai sur tablette.

| Contrôle | Résultat et portée | Rapport |
| --- | --- | --- |
| Autosauvegarde et batterie | **3 362 contrôles réussis** sous Linux avec GCC 13.3 et UndefinedBehaviorSanitizer. Méthodes de production de l’éditeur extraites avec doubles de stockage et d’interface ; inclusion du code batterie de l’interface et compilation du module de protection batterie inchangé. | [autosave-battery-0829.json](tools/verification/autosave-battery-0829.json) |
| Ressources | **PASS** : 174 ressources Factory, 40 ressources du secours firmware, concordance des données embarquées avec la SD et contrôle de syntaxe Python. | [resources-0829.json](tools/verification/resources-0829.json) |
| Langues | **PASS** : 14 packs couvrent les 3 177 clés de référence ; aucune clé manquante ni erreur de format ou glyphe non pris en charge détectée. | [languages-0829.json](tools/verification/languages-0829.json) |
| Wi-Fi et compatibilité C6 | **PASS** sous Linux avec AddressSanitizer et UndefinedBehaviorSanitizer : adaptateur et politique de compatibilité de production, plus les trois méthodes extraites d’activation, de recherche et de connexion Wi-Fi. Absence du symbole RPC de version vérifiée à l’édition de liens. | [compatibility-0829.json](tools/verification/compatibility-0829.json) |
| Syntaxe cible Git/SSH | **PASS** : 247 fichiers contrôlés avec le véritable compilateur RISC-V GCC et les en-têtes/paramètres de la cible, dont les sources PCRE2 et libgit2 corrigées. Il s’agit de contrôles de syntaxe, pas d’essais fonctionnels ni d’une édition de liens. | [vendor-syntax-gcc.json](tools/verification/build-0829/vendor-syntax-gcc.json) |

Les essais de batterie couvrent notamment les mesures invalides, la charge, les seuils de tension et le bouclage du compteur de temps. Ils contrôlent l’absence de sauvegarde déclenchée par l’interface batterie et le maintien de l’autosauvegarde optionnelle. Ils ne rendent pas LVGL et ne mesurent ni l’arrêt ni la recharge d’un appareil réel. Le rapport de langues conserve les différences de séparation en paragraphes signalées ; son résultat ne constitue pas une validation visuelle des traductions.

Les essais de compatibilité couvrent les trois variantes écran, les lectures du diagnostic en cache, des événements concurrents et les parcours Wi-Fi avec différents échecs de surveillance simulés. Les interfaces Wi-Fi, I2C, ESP-Hosted et d’arbitrage réseau sont remplacées par des doubles : ces tests ne prouvent pas une connexion radio réelle ni la compatibilité de tous les firmwares C6. Ils compilent trois méthodes de `Tab5Audio.cpp`, pas ce fichier complet ni le firmware. LeakSanitizer est désactivé en raison d’une limite de l’environnement ; AddressSanitizer et UndefinedBehaviorSanitizer restent actifs.

Aucun résultat de test 0.8.28 n’est automatiquement considéré valide pour un fichier modifié en 0.8.29.
<!-- /test-results -->

## Périmètre de la reprise

- Retrait du refus des recherches et connexions Wi-Fi selon la version majeure C6.
- Retrait de la requête RPC automatique de version C6 avant ces opérations.
- Retrait complet de la sauvegarde de secours de l’éditeur déclenchée par la batterie critique.
- Conservation de l’autosauvegarde ordinaire optionnelle, de la surveillance batterie et de la détection écran.

Les contrôles ciblés Wi-Fi, batterie et éditeur exécutés sont détaillés ci-dessus. Une compilation complète et les contrôles d’image restent distincts des essais hôtes.

## Provenance des corrections GCC

[gcc-source-fixes.json](tools/verification/build-0829/gcc-source-fixes.json) conserve les motifs et les empreintes avant/après des corrections GitView, SDR, PCRE2 et libgit2. [gcc-integer-types.json](tools/verification/build-0829/gcc-integer-types.json) confirme les types entiers de la chaîne cible et la représentabilité des compteurs PCRE2. [dependency-downloads.json](tools/verification/build-0829/dependency-downloads.json) consigne les URL et empreintes des dépendances téléchargées. Ces preuves de provenance et de syntaxe complètent le contrôle final du firmware décrit en tête de ce document.

## Preuves antérieures

[VERIFICATION_0.8.28.md](VERIFICATION_0.8.28.md) conserve le rapport de la livraison précédente. Les rapports sous `tools/verification` gardent leurs noms, dates et empreintes. Les binaires de cette ancienne version ne font pas partie de la livraison 0.8.29.

Une preuve historique ne peut être reprise pour la 0.8.29 que si son périmètre et les empreintes des sources et du banc d’essai correspondent encore. Les tests de la politique C6 et de la copie de secours de la 0.8.28 décrivent précisément les comportements retirés ; ils ne valident pas leur remplacement.

## Essais sur tablette

Aucun flash ni essai sur Tab5 physique n’est revendiqué. Les essais suivants restent à effectuer avec le firmware 0.8.29 livré :

- Vérifier l’activation, la recherche de réseaux et la connexion avec le C6 déjà installé.
- Vérifier que le réglage d’autosauvegarde de l’éditeur est conservé et que son fonctionnement ordinaire suit ce réglage.
- Consulter les informations écran, P4 et batterie ; une version C6 inconnue doit rester un diagnostic, sans interdire le Wi-Fi.

Il n’est pas nécessaire de vider volontairement la batterie pour ces essais. Les anciens retours de redémarrage du coffre, de Git ou d’autres applications restent à confirmer sur matériel ; cette reprise ne revendique pas leur résolution supplémentaire.
