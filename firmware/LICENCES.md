# Licences des composants de Tab5OS 0.8.53

Les notices des composants tiers sont conservées dans leurs dossiers. [LICENCES-manifest.json](LICENCES-manifest.json) donne leurs chemins et empreintes. Cet inventaire décrit les fichiers fournis ; il n'attribue pas une nouvelle licence aux auteurs.

**Aucun fichier de licence principale n'a été trouvé pour les parties propres à Tab5OS.** Son titulaire doit choisir les conditions de redistribution avant de présenter le dépôt comme un projet open source sous une licence déterminée. La préparation du dépôt et de sa documentation ne constitue pas ce choix.

## Composants

| Périmètre | Notices conservées |
| --- | --- |
| Firmware, pilote RTL-SDR | Sources GPL avec notices par fichier : conserver leur code source et leur provenance, examiner la licence du firmware lié |
| Chargeur ELF et pilotes USB Espressif | Apache-2.0, textes conservés ; ne pas les réétiqueter MIT |
| MicroPython et parser Markdown Draftling | MIT, notices conservées |
| Client libssh2 | BSD-3-Clause, notice et provenance dans `src/tab5ssh` |
| libgit2 | GPL-2.0 avec exception de liaison explicite amont ; conserver `COPYING`, `AUTHORS`, `UPSTREAM.json` et les notices par fichier dans `src/tab5git` |
| Décodeurs WebP/Zstandard/XZ, encodeur JPEG, stb | Licences distinctes indiquées dans le manifeste et les fichiers amont |
| Applications ELF GB/GBC/NES/Sega8/GBA/Doom | Plusieurs licences MIT/GPL selon le cœur ; adaptateurs, sources et notices restent fournis avec chaque application |
| MegaDrive / Gwenesis | AGPL-3.0 dans le dossier du cœur ; application ELF distincte, sans reclassement en licence permissive |
| SNES / ndssfc / Snes9x | Notice mixte : GPL pour certaines parties et restriction **non commerciale / usage personnel** Snes9x. Ce composant ne peut pas être annoncé comme intégralement sous une licence open source permissive |
| Police principale DejaVu Sans Mono | Licence Bitstream Vera et modifications DejaVu, présentes dans les métadonnées du TTF et reproduites dans le manifeste |
| Polices CJK et dictionnaires de saisie | OFL-1.1, Apache-2.0 et GPL selon la ressource ; notices et sources des dictionnaires dans `sdcard/System/ime` |

Les fichiers de bibliothèques Arduino externes restent ceux des versions documentées dans [BIBLIOTHEQUES.md](BIBLIOTHEQUES.md). Ils ne deviennent pas des fichiers propres au projet lors de la compilation. Les textes de licence dans les sources, y compris ceux propres à certains fichiers, restent la référence ; une étiquette de tableau ne les remplace pas.

Les textes de licence des composants restent la référence pour leur utilisation et leur redistribution.
