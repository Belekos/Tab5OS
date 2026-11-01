# Tab5OS 0.8.53 — contrôle préalable de publication

L’audit porte sur l’archive livrée `Tab5OS_0.8.53_Explorateur_Bandeau.zip`, SHA-256 `a6c24e92dabb36b4eac18df4c9cff9c63b3f53d0546a54dfd863439fcc56ed58`, et sur ses dépendances de compilation épinglées. Aucun fichier du projet livré ni aucune preuve historique n’a été modifié. Aucun compte public n’a été modifié par cet audit.

## Résultat pratique

La préparation du dépôt, de ses notices, de ses captures et du paquet d’installation peut avancer. Deux décisions distinctes restent nécessaires avant d’annoncer une distribution publique entièrement licenciée : définir la licence des fichiers propres à Tab5OS et clarifier l’association audio réellement liée au firmware. La seconde question ne se résout pas par le seul choix d’une licence pour le projet.

**Aucune licence principale n’est actuellement attribuée aux fichiers propres à Tab5OS.** Une licence GPL-compatible peut être retenue par leur titulaire, par exemple MIT pour ces seuls fichiers, ou GPLv3 pour un périmètre clairement défini. Les licences des tiers restent en place. Une licence MIT sur les fichiers originaux ne rendrait pas le firmware lié intégralement MIT ; la redistribution du firmware doit aussi satisfaire ses dépendances GPL et autres licences.

## Point audio établi

Le précédent manifeste recensait 27 groupes embarqués, mais n’inventoriait pas complètement les dépendances Arduino externes. ESP8266Audio 2.4.2 inclut du code GPLv3 et le décodeur Helix AAC sous RPSL. Le wrapper `AudioGeneratorAAC.cpp` annonce GPLv3-ou-ultérieure. Le fichier `libhelix-aac/aacdec.c` annonce RPSL, ou RCSL seulement pour un titulaire ayant obtenu cette licence séparément.

Ce n’est pas uniquement une présence dans l’arbre des sources : le contrôle de l’ELF final trouve `AACDecode` à `0x402a245a` et `mad_frame_decode` à `0x402aece2`. Le décodeur Helix MP3 n’a pas de symbole `MP3Decode` conservé dans cet ELF ; il ne faut donc pas confondre les objets compilés avec les fonctions réellement liées.

GNU classe RPSL comme incompatible avec GPL. Le texte RPSL, notamment son annexe B, avertit lui-même que les obligations réciproques de GPL peuvent interdire cette combinaison. Aucune exception permettant cette combinaison n’a été trouvée dans les fichiers inspectés. Cela constitue un problème concret de licence du binaire combiné, sans préjuger d’une autorisation distincte que les ayants droit auraient pu accorder.

Les voies pertinentes sont de rechercher une autorisation ou clarification amont couvrant la combinaison, ou d’employer une implémentation AAC compatible après validation technique. La suppression d’AAC n’est pas proposée comme solution automatique : elle retirerait une fonction existante. Aucun changement audio ni nouvelle licence n’a été appliqué dans cette préparation.

Références primaires :

- [Classification GNU de RPSL](https://www.gnu.org/licenses/license-list.html#RPSL).
- [Texte RPSL 1.0 conservé par l’OSI](https://opensource.org/license/RPSL-1.0), sections 4.2 et annexe B.
- [FAQ GNU sur les bibliothèques incompatibles](https://www.gnu.org/licenses/gpl-faq.html#GPLIncompatibleLibs) : une exception pour du code tiers doit provenir des titulaires correspondants.
- [Dépôt amont ESP8266Audio](https://github.com/earlephilhower/ESP8266Audio), qui distingue explicitement le code AAC Helix du reste de sa licence.

## Autres composants à présenter correctement

Les 58 notices du manifeste existant ont été relues ou vérifiées par empreinte ; aucune notice attendue ne manque.

| Périmètre | Conditions à conserver dans la publication |
| --- | --- |
| RTL-SDR dans le firmware | GPLv2-ou-ultérieure : source correspondante, notices, modifications et instructions de construction accessibles. |
| libgit2 | GPLv2 avec exception de liaison explicite ; l’exception autorise les combinaisons exécutables, sans supprimer les obligations sur les fichiers de libgit2 eux-mêmes. |
| MicroPython, Draftling, M5Unified/M5GFX, LVGL, ArduinoJson | Conserver les notices permissives propres aux composants et aux sous-composants ; ne pas leur attribuer la licence du projet par remplacement. |
| Chargeur ELF et pilotes Espressif | Conserver Apache-2.0 et les notices de provenance. |
| ELF Game Boy / GBC / NES / Sega8 / GBA / Doom | Les sources correspondantes, adaptateurs, scripts et licences des moteurs doivent accompagner les exécutables ou être accessibles avec leur version exacte. |
| ELF MegaDrive/Gwenesis | AGPLv3 ; conserver cette licence et les sources correspondantes. Ne pas le reclasser MIT. |
| ELF SNES/ndssfc/Snes9x | La notice permet expressément la distribution gratuite non commerciale en conservant les mentions. Elle comporte une restriction non commerciale : ne pas annoncer tout le paquet comme un ensemble libre sous une licence unique ni comme librement réutilisable commercialement. Ce n’est pas, à elle seule, une interdiction générale d’un dépôt gratuit. |
| GeoNames | Attribution CC-BY-4.0 pour l’index des lieux et les données sources/provenance. |
| Polices et dictionnaires | Conserver les licences DejaVu/Bitstream Vera, OFL, Apache et GPL propres aux fichiers. |

La liste ci-dessus est un inventaire technique et une lecture ciblée des licences, pas une garantie juridique globale. La présence d’un composant dans une archive ne lui attribue pas automatiquement les conditions d’un autre composant.

## Jeux, données et secrets

L’archive exacte contient 8 188 fichiers. Un scan borné de 7 127 fichiers texte n’a trouvé aucun jeton GitHub/OpenAI/Slack aux formats recherchés, clé AWS, bloc complet de clé privée PEM ni adresse personnelle ciblée. Aucun fichier de type `.pem`, `.key`, `.nvs`, base SQLite ou capture réseau n’a été repéré. Les exécutables et images ont été inventoriés, mais ce scan ne constitue pas une preuve absolue d’absence de tout secret possible.

Les seules ROM de jeux repérées dans l’arbre SD sont les démonstrations `Tab5_Pocket.gb` et `Tab5_Color.gbc`, ainsi que leurs copies Factory. Leurs notices les identifient comme créations Tab5OS CC0 ; leurs générateurs sont présents. Aucun jeu commercial ni archive WAD commerciale n’a été identifié. Le BIOS utilisé par le port GBA est présenté dans sa provenance comme un remplacement open source GPL2 ; il n’est pas annoncé comme une extraction du BIOS Nintendo.

## Organisation conseillée pour GitHub

Construire le dépôt depuis l’archive vérifiée. Le dossier de travail local contient encore des temporaires et une ancienne Factory que le paquet livré exclut déjà.

| Dans le dépôt Git | Dans les assets de release ou une archive de preuves |
| --- | --- |
| Sources firmware, sources et scripts des ELF/Python, tests reproductibles | BIN OTA et image d’installation initiale clairement séparés |
| En-têtes générés indispensables à la compilation actuelle | ELF/map de diagnostic, avec empreintes |
| Recettes de compilation, dépendances épinglées, scripts de génération | Paquet SD et copie Factory, avec manifeste/version |
| README, installation FR/EN, limites matérielles, contribution, notices et provenance | Journaux détaillés et captures de tests historiques |
| Captures de présentation sans données personnelles, origine annoncée | Ressources volumineuses générées, notamment index GeoNames |

Le ZIP complet dépasse 1,2 Go décompressé, principalement à cause des preuves et ressources dupliquées. L’historique détaillé est utile pour la traçabilité, mais son placement dans les assets évite de charger durablement l’historique Git. Ne pas retirer les sources ou notices nécessaires à la reconstruction sous prétexte d’alléger le dépôt. Ne pas publier une release contenant seulement le binaire tout en laissant sa source correspondante introuvable.

Les éléments détaillés sont dans `archive-audit.json`, `archive-files.json`, `licence-audit.json` et `linked-audio-symbols.txt`.
