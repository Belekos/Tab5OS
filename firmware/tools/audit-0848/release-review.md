# Revue de cohérence de distribution — 0.8.48

## Sources et gardes de publication

Le script actif `release_config.py` découvre la version du sketch .48. Le manifeste de référence .47 reste authentifié par son SHA-256 ; il n'a pas été réécrit. `assemble_scope.py` doit encore être exécuté par l'intégrateur après gel des dernières modifications et rapports. La validation du paquet exige la compilation complète de cette version, des empreintes stables et des rapports actuels couvrant les sources modifiées. Les rapports historiques restent datés de leur exécution originale.

La liste blanche des seuls quatre fichiers de firmware est déjà appliquée à l'écriture du ZIP et vérifiée après extraction :

- `firmware/Tab5OS_v0.8.48.bin`
- `firmware/SHA256.txt`
- `firmware/debug/Tab5OS_Arduino_v0.8.48_Ecriture.ino.elf`
- `firmware/debug/Tab5OS_Arduino_v0.8.48_Ecriture.ino.map`

Le dossier de travail contenait encore le firmware .47 : il doit être remplacé par les sorties vérifiées .48 avant validation finale. Les anciens `sdcard/Factory/Tab5OS-0.8.47/` sont exclus du ZIP et de l'inventaire de publication. Seul le Factory .48 correspondant à son manifeste est distribué. Aucune commande de suppression n'a été ajoutée au firmware ou à la carte de l'utilisateur.

## Défaut concret de distribution et correction

Des fichiers temporaires de copie à suffixes aléatoires étaient inclus dans les anciens paquets et pouvaient réapparaître pendant la copie du projet. L'inventaire de cette reprise identifie 23 fichiers, totalisant 154 608 630 octets décompressés. Quatre sont déjà exclus par les règles Factory/firmware ; les dix-neuf autres représentent 90 383 350 octets supplémentaires. Les noms, tailles et empreintes sont conservés dans `release-copy-artifacts.json`.

Le nouveau module `tools/release_artifacts.py` ajoute un filtre commun au paquet et à l'inventaire de publication, exclusivement dans ces zones :

- sous `tools/verification/`, `.build.log` et `.<nom>.ppm`, suivis d'un ou plusieurs suffixes alphanumériques de six caractères ;
- dans le seul dossier distribué `sdcard/System/maps/gazetteer/`, `.cities.t5geo` suivi de ce même type de suffixe.

Les fichiers légitimes cachés tels que `.gitignore` ou `.gitlab-ci.yml`, les vrais logs, les références JSON, les PNG, les applications et les documents sont conservés. Il s'agit d'une exclusion de distribution, sans effacement du dossier de travail et sans changement d'accès aux fichiers sur l'appareil.

Quatorze de ces copies, totalisant 69 936 118 octets, figuraient déjà dans l'archive .47 et dans son inventaire historique. Le contrôleur de préservation les distingue maintenant par une liste **exacte** de noms, tailles et empreintes : leur identité est vérifiée contre le manifeste de référence inchangé et consignée dans `excludedHistoricalCopyArtifacts`. Les autres preuves historiques doivent toujours être présentes et inchangées. Cette exception documente des copies accidentelles ; elle ne relabellise ni ne supprime des résultats de tests.

Le manifeste de compilation exclut déjà le `sdcard` et aucun temporaire correspondant n'a été trouvé dans les sources compilées. Cette correction des outils de distribution n'exige pas une nouvelle compilation du firmware.

Validation exécutée : `review_release_filter.py` exécute la véritable boucle de création du paquet sur un petit projet factice, avec la seule validation de firmware remplacée par un contrôleur de fixture. Il vérifie le contenu du ZIP, le CRC et une extraction indépendante : les temporaires et l'ancien Factory/firmware sont absents ; les quatre fichiers autorisés, les vrais logs, les dotfiles légitimes et les ELF restent présents. 84 assertions passent. Rapport `tools/verification/release-copy-filter-0848.json`. **Ce test n'est pas la validation finale du firmware**, laquelle doit encore être exécutée sur les sorties réelles.

## Catalogue ELF : revue sans modification

`Tab5AppCatalogue.cpp` n'exclut pas des ELF selon leur nom d'application. La découverte accepte les fichiers `.elf` (suffixe insensible à la casse) dans `/Applications`, `/Apps`, `/MicroPython` et leurs équivalents USB ; les dossiers applicatifs peuvent contenir `main.app.elf` ou `main.elf`. Les fichiers cachés par convention (nom commençant par un point) restent ignorés.

Le catalogue dynamique est relancé à l'ouverture de la bibliothèque et à la révision du système de fichiers ; une interrogation espacée de cinq secondes est conservée uniquement quand la bibliothèque est affichée pour détecter les écritures externes sans notification. L'accueil natif ne déclenche pas cette interrogation. Les choix d'épinglage de l'utilisateur sont distincts de la découverte.

Limite conservée : le catalogue est borné à 64 entrées et le parcours à 384 éléments ; le dépassement est annoncé et les fichiers supplémentaires restent disponibles dans l'Explorateur. Il ne s'agit pas d'une liste noire nominale, et cette limite n'a pas été changée pendant cette correction. Aucun nouveau défaut bloquant du catalogue n'a été établi lors de cette revue.
