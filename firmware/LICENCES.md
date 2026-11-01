# Licences et provenance — état préparatoire 0.8.22

Les notices des composants tiers sont conservées dans leurs dossiers. [LICENCES-manifest.json](LICENCES-manifest.json) donne leurs chemins et empreintes. Cet inventaire décrit les fichiers fournis ; il n'attribue pas une nouvelle licence aux auteurs.

**Aucun fichier de licence principale n'a été trouvé pour les parties propres à Tab5OS.** Son titulaire doit choisir les conditions de redistribution avant de présenter le dépôt comme un projet open source sous une licence déterminée. La préparation du dépôt et de sa documentation ne constitue pas ce choix.

## Points à traiter pour la publication

| Périmètre | Notice constatée et conséquence pour la préparation |
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

Les anciens textes de versions mentionnent parfois des logiciels retirés. Leur présence dans l'historique n'indique pas qu'ils sont encore distribués. L'inventaire porte sur les répertoires présents, sans ajouter de composants d'après ces anciens textes.

## Vérification technique

Le script d'audit contrôle la présence des notices attendues, calcule leurs empreintes, relève les fichiers dépassant 100 Mio et recherche des formats concrets de clés privées et jetons. Il ne copie jamais les valeurs repérées dans le rapport. Une absence de correspondance n'est pas une preuve absolue d'absence de données personnelles.

Le choix de licence principale et l'examen des composants à restrictions restent des étapes explicites de la publication. Aucune publication GitHub, acceptation de conditions ou suppression de composant n'est effectuée par les scripts de préparation.

Références : [licences de dépôts — documentation GitHub](https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/customizing-your-repository/licensing-a-repository), [licences GNU](https://www.gnu.org/licenses/), [Apache-2.0](https://www.apache.org/licenses/LICENSE-2.0), ainsi que les notices locales indexées dans le manifeste.
