# Compilation et référence 0.8.46

La référence est la livraison exacte `Tab5OS_0.8.45_SD_Radio.zip`, SHA-256 `46fd5da89ebb693a61d3ab34e2d3f3480a714aff76c095ce339f290ee8641573`. Le manifeste est extrait de cette archive, jamais de la copie de travail déjà modifiée. Il comprend 2 390 fichiers du sketch et 2 054 preuves historiques. Ces preuves ne sont ni renommées ni présentées comme de nouvelles exécutions.

État lié de référence : BIN OTA 6 067 488 octets, partition 6 553 600 octets, marge 486 112 octets ; données/BSS internes 93 268 octets. Ces chiffres statiques ne mesurent pas le tas libre ni sa fragmentation. La référence et les sections complètes sont dans `build-baseline.json`.

La voie de compilation conserve Arduino M5Stack 3.3.9, ESP-IDF 5.5.4 et GCC RISC-V 14.2, les bibliothèques verrouillées, l’ABI et les partitions. Les 23 archives et Arduino CLI 1.5.1 ont été vérifiés par empreinte. Les copies isolées de LVGL, M5GFX/JPEG et FS/errno restent actives ; le SDK installé n’est pas modifié.

Les scripts actifs découvrent le sketch courant. `release_config.py` relie désormais la validation au manifeste authentifié de la 0.8.45. La finalisation refuse une source modifiée sans preuve actuelle, une preuve historique modifiée, un fichier absent, un BIN d’une ancienne version ou un paquet ne correspondant pas aux sources réellement compilées.

La génération Factory 0.8.46 préserve les ressources existantes et les anciens hashes nécessaires à leur mise à jour. Elle travaille exclusivement sur les fichiers distribués ; aucun support utilisateur n’est monté. Les 180 ressources, 40 fichiers de récupération intégrés et 14 langues sont contrôlés avant compilation.

## Vérification finale exécutée

Compilation complète réussie, puis reconstruction incrémentale après la dernière correction de synchronisation audio. Dernière invocation : 25 octobre 2026, 21:45:58–21:46:31 UTC ; code retour 0, sources inchangées. Le contrôle final authentifie 2 026 fichiers d’entrée, leurs copies Arduino et les 1 423 objets. Les versions et commandes exactes sont dans `tools/verification/build-0846` après préparation de la livraison.

| Élément lié | 0.8.45 | 0.8.46 |
|---|---:|---:|
| BIN OTA | 6 067 488 | 6 071 360 |
| Partition OTA | 6 553 600 | 6 553 600 |
| Marge OTA | 486 112 | 482 240 |
| Données/BSS internes, somme des deux régions | 93 268 | 93 372 |
| Code IRAM `.iram0.text` | 98 494 | 98 610 |
| TCM texte/données | 794 / 64 | 794 / 64 |
| RTC non initialisée | 1 200 | 1 200 |

Valeurs en octets. La diminution isolée de `.dram0.bss` ne représente pas une économie : des objets se trouvent désormais dans `.dram1.bss`. Le delta total données/BSS est +104 octets, le code IRAM +116 octets ; les alignements sont détaillés dans le rapport et aucun tas libre sur appareil n’est déduit de ces sections. Le retour des 16 Kio de pile audio inactive existait déjà en 0.8.45 et n’est pas compté comme gain nouveau.

SHA-256 du BIN : `adedb383b62bae11b235d72271c8dda6addf8bb3fe04344349150e77ac199443`. ELF32 RISC-V, ABI simple précision, cible ESP32-P4, checksum et SHA embarqué contrôlés. Table de partitions inchangée. Provenance des objets crypto, Bluetooth et interruption DSI contrôlée. Le backport FS exact est vérifié dans les objets et dans les 57 consommateurs de ses en-têtes ; aucun ancien chemin FS compilé.

Le passage distinct d’avertissements retire `-w` sur 14 unités : main, Audio, AudioDriver, Video, Map, DisplayPresent, DisplayCache, DisplaySignals, Storage, VolumeFS, Serial, NetworkGate, MediaList et `Tab5ImageCore.c` (noyau C inclus). `-Wall -Wextra` et les diagnostics fatals de retour, usage non initialisé et usage après libération réussissent. Les mêmes 14 unités authentifiées de la référence produisent 368 avertissements ; les sources finales produisent aussi 368 avertissements, avec mêmes comptes par catégorie pour chaque unité. Les journaux complets restent fournis. Une nouvelle indentation trompeuse Serial a été corrigée avant le gel final ; les avertissements historiques ne sont pas dissimulés ni qualifiés collectivement d’inoffensifs.

Le main généré de référence est authentifié et compilé depuis un répertoire isolé, afin que les inclusions entre guillemets utilisent les sources 0.8.45 authentifiées et non d’anciennes copies de compilation. Les passages intermédiaires réalisés pendant les modifications ont été rejetés par la garde de sources ; ils ne servent pas de preuve finale.

La portée finale est de 18 sources de production modifiées et 18 rapports ciblés actuels. 1 893 sources/fontes restent identiques à la référence, ainsi que 2 054 fichiers de preuves historiques. Le paquet est encore soumis à `prepare_delivery.py`, à l’inventaire de publication et à la validation complète de l’archive réextraite.

Aucun essai sur Tab5 physique n’a été effectué dans cet environnement. Aucun résultat de fluidité, bruit audio, sous-alimentation DSI, consommation énergétique ou mémoire minimale réelle n’est revendiqué.

