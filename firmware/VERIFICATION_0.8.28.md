# Vérification 0.8.28

<!-- build-status -->
**Compilation complète et contrôles de l’image 0.8.28 réussis.** Preuve : [firmware-verification.json](tools/verification/firmware-verification.json).

Les sources compilées et leurs copies Arduino correspondent. Les artefacts sont restés stables pendant les contrôles ; l’image ESP32-P4 passe le checksum et le hash de validation, et son empreinte ELF intégrée correspond à l’ELF compilé.

| Contrôle | Valeur vérifiée |
|---|---|
| Image OTA livrée | `firmware/Tab5OS_v0.8.28.bin` |
| Taille de l’image | 6271264 octets |
| Partition OTA | 6553600 octets |
| Marge | 282336 octets |
| SHA-256 du BIN | `942f4eb682cd3f3f7a770c95af57de87885884532d2a3927c6728f308e6e204f` |

Autres preuves : [build-result.json](tools/verification/build-result.json), [artifact-stability.json](tools/verification/artifact-stability.json), [image-info.log](tools/verification/image-info.log), [toolchain-versions.json](tools/verification/toolchain-versions.json). Le paquet vérifie aussi les fichiers ELF et map livrés dans `firmware/debug`. Aucun essai physique ni téléversement n’a été réalisé.
<!-- /build-status -->

<!-- test-results -->
52 rapports actuels et 47 preuves historiques réutilisées après contrôle des empreintes. Ces nombres dénombrent des rapports, pas des essais physiques.

| Partie | Résultat et portée | Preuve |
|---|---|---|
| wifi-transitions-0828 | Exécution 0.8.28 ; sources et banc de test identifiés par empreinte. | [wifi-transitions-0828.json](tools/verification/wifi-transitions-0828.json) |
| network-gate-0826 | Preuve historique conservée byte à byte ; sources et banc de test identiques vérifiés. | [network-gate-0826.json](tools/verification/network-gate-0826.json) |
| directory-picker-0828 | Exécution 0.8.28 ; sources et banc de test identifiés par empreinte. | [directory-picker-0828.json](tools/verification/directory-picker-0828.json) |
| ui-controls-0828 | Exécution 0.8.28 ; sources et banc de test identifiés par empreinte. | [ui-controls-0828.json](tools/verification/ui-controls-0828.json) |
| calculator-0826 | Preuve historique conservée byte à byte ; sources et banc de test identiques vérifiés. | [calculator-0826.json](tools/verification/calculator-0826.json) |
| reader-layout-0828 | Exécution 0.8.28 ; sources et banc de test identifiés par empreinte. | [reader-layout-0828.json](tools/verification/reader-layout-0828.json) |
| terminal-0826 | Preuve historique conservée byte à byte ; sources et banc de test identiques vérifiés. | [terminal-0826.json](tools/verification/terminal-0826.json) |
| launch-radio-0828 | Exécution 0.8.28 ; sources et banc de test identifiés par empreinte. | [launch-radio-0828.json](tools/verification/launch-radio-0828.json) |
| terminal-stack-0826 | Preuve historique conservée byte à byte ; sources et banc de test identiques vérifiés. | [terminal-stack-0826.json](tools/verification/terminal-stack-0826.json) |
| shell-review-0826 | Preuve historique conservée byte à byte ; sources et banc de test identiques vérifiés. | [shell-review-0826.json](tools/verification/shell-review-0826.json) |
| glyphs-0828 | Exécution 0.8.28 ; sources et banc de test identifiés par empreinte. | [glyphs-0828.json](tools/verification/glyphs-0828.json) |
| arabic-0828 | Exécution 0.8.28 ; sources et banc de test identifiés par empreinte. | [arabic-0828.json](tools/verification/arabic-0828.json) |
| languages-0828 | Exécution 0.8.28 ; sources et banc de test identifiés par empreinte. | [languages-0828.json](tools/verification/languages-0828.json) |
| delta-languages-0828 | Exécution 0.8.28 ; sources et banc de test identifiés par empreinte. | [delta-languages-0828.json](tools/verification/delta-languages-0828.json) |
| ui-locale-calls-0828 | Exécution 0.8.28 ; sources et banc de test identifiés par empreinte. | [ui-locale-calls-0828.json](tools/verification/ui-locale-calls-0828.json) |
| syntax-0828 | Exécution 0.8.28 ; sources et banc de test identifiés par empreinte. | [syntax-0828.json](tools/verification/syntax-0828.json) |
| resources-0828 | Exécution 0.8.28 ; sources et banc de test identifiés par empreinte. | [resources-0828.json](tools/verification/resources-0828.json) |
| maps-0826 | Preuve historique conservée byte à byte ; sources et banc de test identiques vérifiés. | [maps-0826.json](tools/verification/maps-0826.json) |
| gamepad-0824 | Preuve historique conservée byte à byte ; sources et banc de test identiques vérifiés. | [gamepad-0824.json](tools/verification/gamepad-0824.json) |
| wiki-flow-0824 | Preuve historique conservée byte à byte ; sources et banc de test identiques vérifiés. | [wiki-flow-0824.json](tools/verification/wiki-flow-0824.json) |
| reader-seek-0828 | Exécution 0.8.28 ; sources et banc de test identifiés par empreinte. | [reader-seek-0828.json](tools/verification/reader-seek-0828.json) |
| task-diagnostics-0828 | Exécution 0.8.28 ; sources et banc de test identifiés par empreinte. | [task-diagnostics-0828.json](tools/verification/task-diagnostics-0828.json) |
| share-admission-0826 | Preuve historique conservée byte à byte ; sources et banc de test identiques vérifiés. | [share-admission-0826.json](tools/verification/share-admission-0826.json) |
| printer-network-gate-0822 | Preuve historique conservée byte à byte ; sources et banc de test identiques vérifiés. | [printer-network-gate-0822.json](tools/verification/printer-network-gate-0822.json) |
| memory-target-stack-0828 | Exécution 0.8.28 ; sources et banc de test identifiés par empreinte. | [memory-target-stack-0828.json](tools/verification/memory-target-stack-0828.json) |
| resource-inventory-0828 | Exécution 0.8.28 ; sources et banc de test identifiés par empreinte. | [resource-inventory-0828.json](tools/verification/resource-inventory-0828.json) |
| sdk-0825 | Preuve historique conservée byte à byte ; sources et banc de test identiques vérifiés. | [sdk-0825.json](tools/verification/sdk-0825.json) |
| quota-reservation-0826 | Preuve historique conservée byte à byte ; sources et banc de test identiques vérifiés. | [quota-reservation-0826.json](tools/verification/quota-reservation-0826.json) |
| git-0828 | Exécution 0.8.28 ; sources et banc de test identifiés par empreinte. | [git-0828.json](tools/verification/git-0828.json) |
| git-journal-0828 | Exécution 0.8.28 ; sources et banc de test identifiés par empreinte. | [git-journal-0828.json](tools/verification/git-journal-0828.json) |
| git-pull-0822 | Preuve historique conservée byte à byte ; sources et banc de test identiques vérifiés. | [git-pull-0822.json](tools/verification/git-pull-0822.json) |
| git-view-0828 | Exécution 0.8.28 ; sources et banc de test identifiés par empreinte. | [git-view-0828.json](tools/verification/git-view-0828.json) |
| git-tls-lifecycle-0826 | Preuve historique conservée byte à byte ; sources et banc de test identiques vérifiés. | [git-tls-lifecycle-0826.json](tools/verification/git-tls-lifecycle-0826.json) |
| git-init-oom-0826 | Preuve historique conservée byte à byte ; sources et banc de test identiques vérifiés. | [git-init-oom-0826.json](tools/verification/git-init-oom-0826.json) |
| git-regexp-quota-0822 | Preuve historique conservée byte à byte ; sources et banc de test identiques vérifiés. | [git-regexp-quota-0822.json](tools/verification/git-regexp-quota-0822.json) |
| git-stack-0828 | Exécution 0.8.28 ; sources et banc de test identifiés par empreinte. | [git-stack-0828.json](tools/verification/git-stack-0828.json) |
| ssh-0822 | Preuve historique conservée byte à byte ; sources et banc de test identiques vérifiés. | [ssh-0822.json](tools/verification/ssh-0822.json) |
| ssh-target-0822 | Preuve historique conservée byte à byte ; sources et banc de test identiques vérifiés. | [ssh-target-0822.json](tools/verification/ssh-target-0822.json) |
| libgit2-vendor-0826 | Preuve historique conservée byte à byte ; sources et banc de test identiques vérifiés. | [libgit2-vendor-0826.json](tools/verification/libgit2-vendor-0826.json) |
| git-https-0828 | Exécution 0.8.28 ; sources et banc de test identifiés par empreinte. | [git-https-0828.json](tools/verification/git-https-0828.json) |
| terminal-worker-0826 | Preuve historique conservée byte à byte ; sources et banc de test identiques vérifiés. | [terminal-worker-0826.json](tools/verification/terminal-worker-0826.json) |
| git-vendor-compile-0828 | Exécution 0.8.28 ; sources et banc de test identifiés par empreinte. | [git-vendor-compile-0828.json](tools/verification/git-vendor-compile-0828.json) |
| git-no-thread-tls-0826 | Preuve historique conservée byte à byte ; sources et banc de test identiques vérifiés. | [git-no-thread-tls-0826.json](tools/verification/git-no-thread-tls-0826.json) |
| elf-lifecycle-0826 | Preuve historique conservée byte à byte ; sources et banc de test identiques vérifiés. | [elf-lifecycle-0826.json](tools/verification/elf-lifecycle-0826.json) |
| git-config-oom-0826 | Preuve historique conservée byte à byte ; sources et banc de test identiques vérifiés. | [git-config-oom-0826.json](tools/verification/git-config-oom-0826.json) |
| firmware-install-0819 | Preuve historique conservée byte à byte ; sources et banc de test identiques vérifiés. | [firmware-install-0819.json](tools/verification/firmware-install-0819.json) |
| keyboard-shortcuts-0828 | Exécution 0.8.28 ; sources et banc de test identifiés par empreinte. | [keyboard-shortcuts-0828.json](tools/verification/keyboard-shortcuts-0828.json) |
| ime-0828 | Exécution 0.8.28 ; sources et banc de test identifiés par empreinte. | [ime-0828.json](tools/verification/ime-0828.json) |
| ime-native-0828 | Exécution 0.8.28 ; sources et banc de test identifiés par empreinte. | [ime-native-0828.json](tools/verification/ime-native-0828.json) |
| pentest-elf-0825 | Preuve historique conservée byte à byte ; sources et banc de test identiques vérifiés. | [pentest-elf-0825.json](tools/verification/pentest-elf-0825.json) |
| app-panel-0825 | Preuve historique conservée byte à byte ; sources et banc de test identiques vérifiés. | [app-panel-0825.json](tools/verification/app-panel-0825.json) |
| elf-resources-0826 | Preuve historique conservée byte à byte ; sources et banc de test identiques vérifiés. | [elf-resources-0826.json](tools/verification/elf-resources-0826.json) |
| context-menu-0826 | Preuve historique conservée byte à byte ; sources et banc de test identiques vérifiés. | [context-menu-0826.json](tools/verification/context-menu-0826.json) |
| gamepad-pipeline-0824 | Preuve historique conservée byte à byte ; sources et banc de test identiques vérifiés. | [gamepad-pipeline-0824.json](tools/verification/gamepad-pipeline-0824.json) |
| gamepad-settings-0824 | Preuve historique conservée byte à byte ; sources et banc de test identiques vérifiés. | [gamepad-settings-0824.json](tools/verification/gamepad-settings-0824.json) |
| elf-api5-0826 | Preuve historique conservée byte à byte ; sources et banc de test identiques vérifiés. | [elf-api5-0826.json](tools/verification/elf-api5-0826.json) |
| explorer-0828 | Exécution 0.8.28 ; sources et banc de test identifiés par empreinte. | [explorer-0828.json](tools/verification/explorer-0828.json) |
| git-close-baseline-0828 | Exécution 0.8.28 ; sources et banc de test identifiés par empreinte. | [git-close-baseline-0828.json](tools/verification/git-close-baseline-0828.json) |
| app-panel-ui-0828 | Exécution 0.8.28 ; sources et banc de test identifiés par empreinte. | [app-panel-ui-0828.json](tools/verification/app-panel-ui-0828.json) |
| consoles-ui-0828 | Exécution 0.8.28 ; sources et banc de test identifiés par empreinte. | [consoles-ui-0828.json](tools/verification/consoles-ui-0828.json) |
| folder-preview-0824 | Preuve historique conservée byte à byte ; sources et banc de test identiques vérifiés. | [folder-preview-0824.json](tools/verification/folder-preview-0824.json) |
| map-session-0826 | Preuve historique conservée byte à byte ; sources et banc de test identiques vérifiés. | [map-session-0826.json](tools/verification/map-session-0826.json) |
| system-migration-0824 | Preuve historique conservée byte à byte ; sources et banc de test identiques vérifiés. | [system-migration-0824.json](tools/verification/system-migration-0824.json) |
| sdr-worker-0826 | Preuve historique conservée byte à byte ; sources et banc de test identiques vérifiés. | [sdr-worker-0826.json](tools/verification/sdr-worker-0826.json) |
| sdr-ui-0826 | Preuve historique conservée byte à byte ; sources et banc de test identiques vérifiés. | [sdr-ui-0826.json](tools/verification/sdr-ui-0826.json) |
| sdr-usb-0826 | Preuve historique conservée byte à byte ; sources et banc de test identiques vérifiés. | [sdr-usb-0826.json](tools/verification/sdr-usb-0826.json) |
| media-0828 | Exécution 0.8.28 ; sources et banc de test identifiés par empreinte. | [media-0828.json](tools/verification/media-0828.json) |
| gamepad-navigation-0824 | Preuve historique conservée byte à byte ; sources et banc de test identiques vérifiés. | [gamepad-navigation-0824.json](tools/verification/gamepad-navigation-0824.json) |
| gamepad-history-0824 | Preuve historique conservée byte à byte ; sources et banc de test identiques vérifiés. | [gamepad-history-0824.json](tools/verification/gamepad-history-0824.json) |
| numeric-keyboard-0828 | Exécution 0.8.28 ; sources et banc de test identifiés par empreinte. | [numeric-keyboard-0828.json](tools/verification/numeric-keyboard-0828.json) |
| system-paths-0826 | Preuve historique conservée byte à byte ; sources et banc de test identiques vérifiés. | [system-paths-0826.json](tools/verification/system-paths-0826.json) |
| settings-recovery-0828 | Exécution 0.8.28 ; sources et banc de test identifiés par empreinte. | [settings-recovery-0828.json](tools/verification/settings-recovery-0828.json) |
| credential-ui-0828 | Exécution 0.8.28 ; sources et banc de test identifiés par empreinte. | [credential-ui-0828.json](tools/verification/credential-ui-0828.json) |
| vault-0828 | Exécution 0.8.28 ; sources et banc de test identifiés par empreinte. | [vault-0828.json](tools/verification/vault-0828.json) |
| vault-target-0828 | Exécution 0.8.28 ; sources et banc de test identifiés par empreinte. | [vault-target-0828.json](tools/verification/vault-target-0828.json) |
| runtime-env-0828 | Exécution 0.8.28 ; sources et banc de test identifiés par empreinte. | [runtime-env-0828.json](tools/verification/runtime-env-0828.json) |
| settings-layout-0828 | Exécution 0.8.28 ; sources et banc de test identifiés par empreinte. | [settings-layout-0828.json](tools/verification/settings-layout-0828.json) |
| sdr-dsp-0826 | Preuve historique conservée byte à byte ; sources et banc de test identiques vérifiés. | [sdr-dsp-0826.json](tools/verification/sdr-dsp-0826.json) |
| help-0828 | Exécution 0.8.28 ; sources et banc de test identifiés par empreinte. | [help-0828.json](tools/verification/help-0828.json) |
| git-focus-baseline-0826 | Preuve historique conservée byte à byte ; sources et banc de test identiques vérifiés. | [git-focus-baseline-0826.json](tools/verification/git-focus-baseline-0826.json) |
| git-tls-stream-0826 | Preuve historique conservée byte à byte ; sources et banc de test identiques vérifiés. | [git-tls-stream-0826.json](tools/verification/git-tls-stream-0826.json) |
| maps-search-0826 | Preuve historique conservée byte à byte ; sources et banc de test identiques vérifiés. | [maps-search-0826.json](tools/verification/maps-search-0826.json) |
| maps-target-0826 | Preuve historique conservée byte à byte ; sources et banc de test identiques vérifiés. | [maps-target-0826.json](tools/verification/maps-target-0826.json) |
| serial-backend-0827 | Preuve historique conservée byte à byte ; sources et banc de test identiques vérifiés. | [serial-backend-0827.json](tools/verification/serial-backend-0827.json) |
| serial-text-0827 | Preuve historique conservée byte à byte ; sources et banc de test identiques vérifiés. | [serial-text-0827.json](tools/verification/serial-text-0827.json) |
| serial-syntax-0828 | Exécution 0.8.28 ; sources et banc de test identifiés par empreinte. | [serial-syntax-0828.json](tools/verification/serial-syntax-0828.json) |
| writing-serial-0828 | Exécution 0.8.28 ; sources et banc de test identifiés par empreinte. | [writing-serial-0828.json](tools/verification/writing-serial-0828.json) |
| vault-reopen-0828 | Exécution 0.8.28 ; sources et banc de test identifiés par empreinte. | [vault-reopen-0828.json](tools/verification/vault-reopen-0828.json) |
| battery-protection-0828 | Exécution 0.8.28 ; sources et banc de test identifiés par empreinte. | [battery-protection-0828.json](tools/verification/battery-protection-0828.json) |
| battery-ui-0828 | Exécution 0.8.28 ; sources et banc de test identifiés par empreinte. | [battery-ui-0828.json](tools/verification/battery-ui-0828.json) |
| battery-integration-0828 | Exécution 0.8.28 ; sources et banc de test identifiés par empreinte. | [battery-integration-0828.json](tools/verification/battery-integration-0828.json) |
| vault-buffers-0828 | Exécution 0.8.28 ; sources et banc de test identifiés par empreinte. | [vault-buffers-0828.json](tools/verification/vault-buffers-0828.json) |
| crypto-lock-0828 | Exécution 0.8.28 ; sources et banc de test identifiés par empreinte. | [crypto-lock-0828.json](tools/verification/crypto-lock-0828.json) |
| crypto-lock-target-0828 | Exécution 0.8.28 ; sources et banc de test identifiés par empreinte. | [crypto-lock-target-0828.json](tools/verification/crypto-lock-target-0828.json) |
| crypto-lock-link-0828 | Exécution 0.8.28 ; sources et banc de test identifiés par empreinte. | [crypto-lock-link-0828.json](tools/verification/crypto-lock-link-0828.json) |
| writing-recovery-0828 | Exécution 0.8.28 ; sources et banc de test identifiés par empreinte. | [writing-recovery-0828.json](tools/verification/writing-recovery-0828.json) |
| git-target-0828 | Exécution 0.8.28 ; sources et banc de test identifiés par empreinte. | [git-target-0828.json](tools/verification/git-target-0828.json) |
| compatibility-0828 | Exécution 0.8.28 ; sources et banc de test identifiés par empreinte. | [compatibility-0828.json](tools/verification/compatibility-0828.json) |
| compatibility-target-0828 | Exécution 0.8.28 ; sources et banc de test identifiés par empreinte. | [compatibility-target-0828.json](tools/verification/compatibility-target-0828.json) |

Les 14 packs couvrent les 3178 clés de référence contrôlées. Factory contient 174 ressources officielles ; le secours firmware en contient 40. Les grandes polices et les dictionnaires de saisie sont distribués sur SD. Preuves : [languages-0828.json](tools/verification/languages-0828.json), [ime-0828.json](tools/verification/ime-0828.json), [resources-0828.json](tools/verification/resources-0828.json).

Les rapports historiques conservent leurs dates, versions, résultats et limites. Le renommage du dossier de sketch permet seulement de retrouver les mêmes fichiers : leurs empreintes doivent rester strictement identiques. Les preuves concernant une source modifiée sont refusées. Les anciens tests Pentest natifs ne servent pas de preuve à l’application ELF : les nouveaux rapports vérifient son code, le panneau OS et leurs frontières. Le gate [features-0828.json](tools/verification/features-0828.json) consigne chaque preuve réellement acceptée et chaque remplacement. La compilation complète porte sur la totalité de la livraison. Aucun essai physique, mesure SRAM sur Tab5 ni téléversement n’est revendiqué.
<!-- /test-results -->

## Retour matériel et essais restant à faire

Les photos de la version 0.8.27 indiquent « watchdog général ». Les valeurs de mémoire ont été relevées après redémarrage : elles ne permettent pas de conclure sur la mémoire au moment du blocage. La mesure batterie affichée, 1,04 V et 0 mA, ne constitue pas un niveau de charge exploitable ; ce cas invalide est couvert par les essais de la protection et ne provoque pas d'arrêt automatique.

La validation physique de cette version reste nécessaire :

- Déverrouiller les clés et le coffre, puis fermer et rouvrir leur interface. Les essais de réinitialisation destructrice ont été réalisés sur des données simulées, pas sur les données de l'utilisateur.
- Cloner un dépôt dans un nouveau dossier, puis vérifier son état depuis l'explorateur et le terminal.
- Ouvrir **Affichage > Série** dans l'éditeur et vérifier l'envoi de la sélection, ou du document entier sans sélection, ainsi que la réception.
- Consulter **Informations système** pour le modèle écran/tactile, les versions P4/C6 et les mesures batterie. La coupure et la recharge réelles n'ont pas été testées sur une tablette ; il n'est pas nécessaire de vider volontairement la batterie pour essayer cette livraison.

Si un redémarrage persiste, relever immédiatement **Dernier reset** et **Avant le reset (contexte)**, ainsi que l'action effectuée. Les marqueurs RTC facilitent ce diagnostic sans enregistrer les identifiants, mots de passe, URL ou chemins Git/coffre.
