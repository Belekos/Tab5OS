# Tab5OS 0.8.53

Un environnement autonome pour **M5Stack Tab5 / ESP32-P4** : écriture, fichiers, ebooks, cartes hors ligne, Wikipedia, audio, terminal et applications C/ELF ou MicroPython. Le firmware C/C++ partage un seul affichage LVGL et les services de la tablette.

Ce projet cible le matériel Tab5. Ce n'est ni Linux ni un émulateur de PC : un programme Linux, Windows ou ESP32 d'une autre architecture ne s'exécute pas directement dessus.

La 0.8.53 reprend la 0.8.52 livrée : survol des menus de l’explorateur, accès rapides aux images, icône du réveil, clavier virtuel Meshtastic, raccourcis latéraux pour applications natives/ELF/Python et économies SRAM ciblées. Le [rapport fonctionnel](RAPPORT_FONCTIONNEL_0.8.53.md) et les [essais Tab5](TESTS_TAB5_0.8.53.md) précisent les changements et leurs limites.

<!-- build-status -->
**Compilation complète ESP32-P4 et vérification du BIN réussies.** Tests ciblés et limites dans [VERIFICATION.md](VERIFICATION.md). Aucun essai physique effectué.
<!-- /build-status -->

Les corrections 0.8.29 sont conservées : aucune interdiction Wi-Fi fondée sur le numéro de version C6, aucune sauvegarde forcée de l’éditeur sur batterie critique. L’autosauvegarde ordinaire reste optionnelle. Les notes antérieures gardent leur portée historique ; voir [COMPATIBILITE_TAB5_0.8.29.md](COMPATIBILITE_TAB5_0.8.29.md) pour le fonctionnement introduit en 0.8.29 et ses limites matérielles.

## Commencer

- **Installer ou mettre à jour** : [LISEZ_MOI.md](LISEZ_MOI.md). Firmware et ressources SD doivent correspondre à la même version.
- **English installation guide**: [READ_FIRST_EN.txt](READ_FIRST_EN.txt).
- **Recompiler le firmware** : [COMPILATION.md](COMPILATION.md) et la [procédure Linux/GCC](tools/build_linux/README_LINUX.md). Les alternatives Windows restent fournies ; `Compiler_Tab5OS.ps1 -CheckOnly` vérifie le sketch sélectionné sans installer, compiler ni flasher.
- **Créer une application** : [DEVELOPPEMENT.md](DEVELOPPEMENT.md), exemples C dans `Tab5OS_Arduino_v0.8.53_Ecriture/tools/native-apps/examples`, exemple Python dans `examples/micropython/compteur`.
- **Comprendre les résultats et les limites** : [VERIFICATION.md](VERIFICATION.md), rapports reproductibles sous `tools/verification`.

## Organisation

| Emplacement | Usage |
| --- | --- |
| `Tab5OS_Arduino_v0.8.53_Ecriture` | Sketch complet, services natifs et bibliothèques embarquées |
| `Tab5OS_Arduino_v0.8.53_Ecriture/sdcard` | Ressources à copier sur la microSD, modules Python et applications fournies |
| `tools` | Vérifications, préparation des ressources et compilation Linux/GCC ou Windows/Clang |
| `examples` | Petits projets à copier et adapter |
| `LICENCES-manifest.json` | Notices amont et empreintes des fichiers examinés |

## Fonctionnement

L'OS possède l'écran, le clavier, la souris, les manettes, l'audio et les supports de stockage. Les applications utilisent ses API ; elles n'initialisent pas une deuxième instance LVGL ou un autre pilote USB. Les opérations longues doivent rendre la main et accepter l'annulation. Le SDK ELF réserve ses blocs suivis en PSRAM, puis l'OS les récupère après la fin effective du programme.

Les thèmes, traductions et raccourcis sont communs aux interfaces natives. Les modèles Python utilisent `tab5ui.tr()` et les widgets de l'OS. HID et XInput sont deux transports USB disponibles ; voir [MANETTES_USB.md](MANETTES_USB.md).

Les tests sur ordinateur contrôlent des calculs, des parcours d'erreur et des allocations. Ils ne remplacent pas les essais tactiles, radio, USB et audio sur le Tab5. Un ELF partage la mémoire du firmware : un accès invalide dans une application native peut encore provoquer un redémarrage. Aucun isolement matériel de processus n'est annoncé.

## Contribuer et publier

Lire [CONTRIBUTING.md](CONTRIBUTING.md) pour les changements, preuves et rapports de défaut. Cette arborescence est préparée pour un dépôt GitHub ; sa préparation ne crée pas de dépôt distant et ne publie rien.

**La licence des fichiers propres au projet reste à choisir par leur titulaire.** Les licences amont sont conservées. Certains composants sont sous GPL/AGPL et l'émulateur SNES contient une restriction non commerciale : ne pas présenter l'ensemble comme couvert par une licence MIT ou équivalente. [LICENCES.md](LICENCES.md) détaille ces points avant publication.

### English

Tab5OS is a standalone ESP32-P4 environment for the M5Stack Tab5. Build sources, SD assets and third-party notices are included. Native ELF applications use the bounded cooperative Tab5 ABI; Python applications use the OS UI and storage services. Neither interface provides Linux process isolation. See the installation guide above and the tested SDK examples. The main project license has not yet been selected; bundled third-party licenses and restrictions remain applicable.
