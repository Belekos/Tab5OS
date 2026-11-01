# Tab5OS

**Écrire, dessiner et programmer sur M5Stack Tab5.**

[English](README.md) · [Installation](docs/INSTALL_FR.md) · [Compilation](docs/BUILD.md) · [Galerie](docs/screenshots/README.fr.md) · [Évolutions 0.8.53](RELEASE_NOTES.md)

![Aperçus de l’interface Tab5OS](docs/screenshots/cover-fr.png)

Tab5OS est un firmware graphique pour le **M5Stack Tab5 / ESP32-P4**. Il réunit documents, création, fichiers, médias et applications programmables dans une interface utilisable au doigt, au clavier USB et à la souris.

Ce dépôt contient les **sources 0.8.53** et leur documentation de publication. Les visuels sont des **rendus LVGL sur ordinateur**, avec des données de démonstration ; ce ne sont pas des captures d’une tablette physique. Leur [provenance](docs/screenshots/manifest.json) est fournie.

## Les outils

| Usage | Fonctions |
| --- | --- |
| Écrire et lire | Éditeur de texte/code, ebooks et ressources Wikipédia locales |
| Dessiner | Calques, sélections, pinceaux, texte, remplissages, historique et projets T5I ; export BMP/JPEG |
| Gérer les fichiers | Explorateur SD/USB, actions contextuelles et choix de l’application d’ouverture |
| Écouter et explorer | Lecture multimédia, webradio, cartes en ligne/hors ligne et repères |
| Programmer | Terminal, outils Git, MicroPython et applications natives ELF |
| Connecter et personnaliser | Meshtastic, outils USB, RTL-SDR, réveil, thèmes et 14 langues d’interface |

L’accueil natif comprend Explorateur, Éditeur, eBook, Média, Cartes, Calculatrice, Réglages, Terminal, Réveil, RTL-SDR, Dessin et Meshtastic. Le bandeau accueille jusqu’à **dix raccourcis natifs, ELF ou Python sans défilement**. Les applications externes sont découvertes dans les emplacements SD/USB pris en charge.

Meshtastic demande une radio externe compatible ; RTL-SDR demande un récepteur USB compatible. Les ELF doivent cibler l’ABI Tab5OS et partagent l’espace mémoire du firmware : ils ne sont pas isolés comme des processus.

## Installer ou mettre à jour

Consulter le [guide d’installation](docs/INSTALL_FR.md). Il distingue deux usages :

- **Tab5OS déjà installé :** utiliser le BIN applicatif OTA depuis la mise à jour intégrée pour conserver NVS et table de partitions.
- **Première installation :** utiliser l’image P4 fusionnée uniquement si le remplacement du contenu de la Flash interne est voulu.

Les ressources microSD de la même version sont nécessaires aux applications externes, polices, langues et données hors ligne. Ne pas remplacer tout le dossier `/System` d’une carte contenant des réglages personnels.

**État de publication :** ce dépôt contient les sources. Il ne propose pas de firmware ou d’ELF précompilé et n’annonce pas une fiche Tab5OS déjà disponible dans M5Burner. Le choix de licence du code propre et la compatibilité des licences des composants audio liés restent à résoudre. Voir [NOTICE.md](NOTICE.md) et l’[audit de publication](docs/publication/AUDIT_PUBLICATION_0.8.53.md).

## Compiler et parcourir les sources

Le projet existant est conservé sous [`firmware/`](firmware/), avec ses chemins relatifs. Le sketch courant se trouve dans [`firmware/Tab5OS_Arduino_v0.8.53_Ecriture`](firmware/Tab5OS_Arduino_v0.8.53_Ecriture/).

La compilation vérifiée utilise M5Stack Arduino **3.3.9 / ESP-IDF 5.5.4**, GCC RISC-V **14.2.0**, LVGL **9.6.0**, M5Unified **0.2.23** et M5GFX **0.2.30**. Les scripts fournis préparent les adaptations isolées des dépendances nécessaires au firmware. Le [guide de compilation](docs/BUILD.md) donne la séquence exacte.

Le [plan du dépôt](docs/REPOSITORY_LAYOUT.md) explique les exclusions : binaires, ELF/map de débogage, Factory dupliquée et volumineux journaux historiques. Les sources, notices tierces et recettes de compilation conservent leurs octets d’origine ; le [manifeste](docs/publication/source-manifest.json) permet de les comparer à l’archive vérifiée.

## Vérifications

La livraison 0.8.53 a passé la compilation complète ESP32-P4 et les tests hôtes ciblés. Son image applicative liée mesurait 6 113 184 octets, avec 440 416 octets de marge OTA. DATA+BSS internes occupaient 91 508 octets, soit 2 620 octets de moins que la 0.8.52. Ces chiffres décrivent les sections liées, pas la mémoire libre en fonctionnement.

Le [compte rendu](firmware/VERIFICATION.md) distingue les contrôles exécutés, une ancienne attente de test de pagination déjà incorrecte et les essais matériels restants. Aucun test sur Tab5 physique ni import M5Burner n’est revendiqué pour cette préparation. Le [protocole sur appareil](firmware/TESTS_TAB5_0.8.53.md) couvre entrées, raccourcis, mémoire et périphériques.

## Participer

Voir [CONTRIBUTING.md](CONTRIBUTING.md). Pour un défaut, préciser version, révision matérielle, périphériques et étapes de reproduction ; joindre les diagnostics utiles sans mot de passe, PIN ou clé. La stabilité et la conservation des données passent avant les nouvelles fonctions.

Les composants tiers conservent leurs propres licences. Aucune licence unique n’est attribuée à tout l’arbre ; voir [NOTICE.md](NOTICE.md).
