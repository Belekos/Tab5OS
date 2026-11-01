# Cartes SD : capacité, format et mémoire

Les mécanismes de stockage décrits ici sont conservés en 0.8.21, avec le paquet M5Stack 3.3.9 et ESP-IDF 5.5.4. Les mesures de structures ci-dessous proviennent de l'audit initial 0.8.5 ; les essais propres à la version courante figurent dans `VERIFICATION.md`. La récupération des ressources système et la police de secours sont décrites dans `REPARATION_SD.md`.

**Une carte de plus de 32 Go ne nécessite pas forcément de changer de système de fichiers.** Pour cette version, le choix pratique reste une partition FAT32 sur une carte de 64, 128 ou 256 Go. Le pilote actuel sait adresser ces capacités ; chaque modèle de carte reste à vérifier sur la tablette. Cette livraison ne formate aucune carte et n'ajoute pas le support exFAT.

## Deux limites différentes

| Élément | Limite ou comportement |
| --- | --- |
| Capacité de la carte | 32 Go est la frontière entre les normes SDHC et SDXC ; ce n'est pas la capacité maximale du format FAT32. |
| Fichier individuel en FAT32 | Au maximum 4 294 967 295 octets, soit 4 Gio moins un octet. |
| Cartes SDXC neuves | Généralement formatées en exFAT, format que le firmware livré ne sait pas monter. Une carte non reconnue n'est donc pas nécessairement trop grande. |
| Bibliothèque Wikipédia Tab5OS | Archives découpées en segments de 64 Mio, avec une taille totale et des positions logiques sur 64 bits. L'encyclopédie entière peut dépasser 4 Gio. |
| Tuiles cartographiques | PNG séparés ou paquets de 1 900 Mio par défaut (2 000 Mio maximum), car les positions de lecture du SDK sont signées sur 32 bits. Jusqu’à 128 paquets sont reconnus sur la SD. |

Le pilote SPI de cette compilation utilise des numéros de secteurs sur 32 bits et des secteurs de 512 octets. Il n'a pas de limite à 32 Go ; sa limite d'adressage est de l'ordre de 2 Tio. Cela ne vaut pas certification des cartes de 2 To : le calcul du nombre de secteurs atteint lui-même sa limite à 2³² secteurs. Les cartes SDUC au-delà de 2 To ne sont pas une cible de cette version.

La page Espressif liée dans la discussion est une documentation évolutive ; sa branche `stable` peut différer du SDK compilé ici. Le tableau mentionne notamment une limite théorique de FAT32 qui n'est pas la limite du pilote SPI compilé ici. Pour cette analyse, les en-têtes et bibliothèques réellement utilisés par le firmware ont aussi été examinés.

## Le format ne libère pas la RAM des applications

Le contenu de la carte et la table d'allocation entière ne sont pas chargés en RAM. Passer de 32 à 128 Go ne multiplie donc pas par quatre la mémoire réservée au système de fichiers. Une grande carte peut en revanche rendre plus longues certaines recherches de place libre ou opérations sur des répertoires très remplis.

Vérification des réglages compilés et des tailles de structures avec le compilateur RISC-V du firmware :

| Réglage ou structure | Valeur constatée | Conséquence |
| --- | --- | --- |
| `FF_FS_EXFAT` | `0` | exFAT désactivé. |
| `FF_LBA64` | `0` | Adressage des secteurs sur 32 bits. |
| `FSIZE_t`, `off_t`, `size_t` | 4 octets chacun | Les gros fichiers exigeraient aussi un audit des API, pas seulement l'activation d'exFAT. `off_t` est signé. |
| Cache privé par fichier | Activé | Les fichiers disposent de leur propre cache de secteur. |
| Taille maximale de secteur configurée | 4 096 octets | Les caches sont dimensionnés pour 4 Kio, même lorsque la carte utilise des secteurs de 512 octets. |
| `sizeof(FIL)` | 4 136 octets | Coût d'un emplacement de fichier dans le montage FatFs. |
| `sizeof(FATFS)` | 4 152 octets | Structure du volume, hors autres éléments du contexte VFS. |
| Nombre maximal de fichiers du montage | 24 | 99 264 octets, environ 97 Kio, réservés pour ces emplacements dès le montage. |
| Allocation FatFs | PSRAM préférée, SRAM en secours | Ces réservations utilisent normalement la PSRAM disponible. |
| Noms longs | Tampon de 512 octets sur la pile | Coût temporaire à prendre en compte dans les tâches qui accèdent aux fichiers. |

Les 24 emplacements sont réservés au montage, indépendamment du nombre de fichiers actuellement ouverts. Fermer une application ferme ses fichiers et libère ses propres buffers, mais ne détruit pas le contexte commun de la carte. Cette réserve n'est donc pas une fuite du cache de l'application précédente.

Réduire arbitrairement les 24 emplacements ferait surtout gagner quelques dizaines de Kio de PSRAM, avec un risque de manquer de fichiers ouverts lorsque plusieurs fonctions sont actives. Cela ne résout pas les pénuries de SRAM des décodeurs, piles de tâches ou transferts DMA.

Une reconstruction contrôlée du SDK pourrait activer les caches adaptés à la taille réelle des secteurs (`CONFIG_FATFS_USE_DYN_BUFFERS`), actuellement désactivés. C'est une optimisation plus pertinente que changer le format de la carte, mais elle n'est pas activable correctement par un simple `#define` dans le programme lorsque FatFs est déjà précompilé.

## exFAT et LittleFS : intérêt réel

**exFAT** serait utile pour copier directement de très gros fichiers uniques et conserver la compatibilité PC. Il n'est pas un remède à la consommation de SRAM. L'activation change les structures de FatFs, les tailles et les positions de fichiers ; changer seulement un en-tête sans recompiler les bibliothèques correspondantes serait incorrect. Il faudrait reconstruire la pile de stockage compatible, puis adapter et vérifier les chemins Arduino, POSIX, lecteurs multimédias et Python qui utilisent encore des positions sur 32 bits. Les archives segmentées permettent déjà les grosses bibliothèques Wikipédia sans cette migration.

**LittleFS** présente un intérêt pour la résistance aux coupures et la maîtrise des caches. Il est peu pratique pour une carte amovible qui doit être remplie directement sous Windows : le PC ne la monte pas nativement comme FAT32 ou exFAT. Le changer ne corrigerait pas une perte de communication SPI, une alimentation instable ou un démontage concurrent. Ce n'est donc pas le format retenu pour la carte de documents, cartes et encyclopédies.

## Utilisation conseillée pour cette version

Conserver FAT32, y compris sur une carte supérieure à 32 Go. Pour préparer une nouvelle carte, sauvegarder auparavant ses données, utiliser une partition FAT32 principale de type MBR, puis recopier les ressources SD de Tab5OS. L'outil de formatage doit effectivement proposer FAT32 pour toute la capacité : les possibilités de l'interface Windows et de sa commande de formatage dépendent de la version de Windows. Le simple choix « format par défaut » d'un outil pour carte SDXC peut produire de l'exFAT.

Le partage Wi-Fi de cette version utilise aussi ces positions signées : les transferts sont limités à 2 Gio moins un octet par fichier. Les segments Wikipédia de 64 Mio et les nouveaux volumes cartographiques restent compatibles.

Ne pas changer le format d'une carte qui fonctionne dans le seul but d'économiser de la RAM. Pour les archives Wikipédia reçues sur PC, utiliser l'import ou le découpage fourni dans le projet ; copier un ZIM de plusieurs dizaines de Go en un seul fichier sur FAT32 n'est pas possible.

La fréquence SPI de cette version est fixée à 10 MHz. Les téléchargements réessaient les erreurs transitoires. Un programme Python peut relâcher temporairement son droit d’accès au stockage pour laisser l’interface remonter la SD ; cette opération exige la fermeture de tous ses fichiers et itérateurs de dossier. Un défaut réseau possède un code distinct d’une erreur SD.

Les sauvegardes de secours des documents sont placées dans `/.tab5-backups`. L’explorateur masque les noms commençant par un point et les fichiers `.bak` ; Ctrl+H ou la commande discrète d’affichage des éléments cachés les rend visibles. L’œil masque ou réaffiche une sélection sans la renommer ; cette préférence Tab5OS ne modifie pas les attributs Windows.

La récupération automatique de Tab5OS reste bornée et attend la fermeture des utilisateurs de la carte avant un remontage. Elle ne peut pas réparer les structures FAT endommagées par une coupure pendant une écriture. Dans ce cas, sauvegarder les fichiers lisibles et vérifier la carte depuis le PC avant de reprendre les téléchargements.

## Sources et provenance

- [Espressif : choix des systèmes de fichiers, version 5.5.4](https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32p4/api-guides/file-system-considerations.html).
- [Espressif : même guide, branche stable](https://docs.espressif.com/projects/esp-idf/en/stable/esp32p4/api-guides/file-system-considerations.html).
- [FatFs : limites et usage mémoire](https://elm-chan.org/fsw/ff/doc/appnote.html), [options de configuration](https://elm-chan.org/fsw/ff/doc/config.html).
- [SD Association : normes de capacité SD, SDHC, SDXC, SDUC](https://www.sdcard.org/developers/sd-standard-overview/capacity-sd-sdhc-sdxc-sduc/).
- [M5Stack : utilisation de la microSD sur Tab5 en Arduino](https://docs.m5stack.com/en/arduino/m5tab5/microsd).
- [Espressif : Arduino comme composant ESP-IDF et reconstruction du cœur](https://docs.espressif.com/projects/arduino-esp32/en/latest/esp-idf_component.html).
- Audit local : `sdkconfig`, `ffconf.h`, `ff.h`, `SD.cpp`, `sd_diskio.cpp` du paquet M5Stack 3.3.9 utilisé, plus [`vfs_fat.c`](https://github.com/espressif/esp-idf/blob/v5.5.4/components/fatfs/vfs/vfs_fat.c) et [`ffsystem.c`](https://github.com/espressif/esp-idf/blob/v5.5.4/components/fatfs/port/freertos/ffsystem.c). Les tailles de structures ci-dessus ont été vérifiées par compilation pour la cible ESP32-P4 de ce projet, pas déduites d'une compilation PC.
