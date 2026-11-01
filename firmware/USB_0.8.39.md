# USB dans Tab5OS 0.8.39

## Formatage explicite d’un stockage

Les réglages proposent FAT (FAT12 ou FAT16, 1 Mio à 2 Gio) et FAT32 (64 Mio à 2 Tio). Ces bornes sont une politique conservatrice ; la géométrie réelle et la réussite de FatFS restent vérifiées. Les secteurs acceptés font 512, 1 024, 2 048 ou 4 096 octets. La configuration FatFS du SDK n’active pas exFAT : cette version ne propose donc ni exFAT ni NTFS.

Le support peut avoir un système de fichiers inconnu ou endommagé. Le pilote identifie sa capacité sans le monter. Après confirmation, il bloque les nouvelles ouvertures, attend la fermeture des fichiers et l’accès exclusif au stockage, puis synchronise et démonte le volume éventuel. L’attente est annulable ; l’écriture commencée ne l’est plus. Une génération de connexion lie la confirmation au support présent : une reconnexion invalide l’ancienne confirmation.

Une tâche distincte utilise un tampon de travail de 4 Kio et une structure FATFS en PSRAM. Elle efface les en-têtes de partition en début et fin de support, demande à FatFS une partition MBR unique et deux copies de la FAT, puis vérifie le type créé avant le remontage. Cette opération remplace le partitionnement de tout le support ; ce n’est pas un effacement sécurisé des données. Le montage automatique conserve `format_if_mount_failed=false`. Aucun formatage n’est déclenché automatiquement.

Le handle USB reste détenu jusqu’au retour du worker, même lors d’un retrait. `pending()` reste vrai pendant le travail, ce qui bloque aussi la mise en veille. Les erreurs de fermeture, les références actives, les refus d’allocation et le conflit avec un vrai dossier `/USB` de la microSD conservent leurs garde-fous.

## Imprimantes IPP-over-USB

Le pilote complète IPP/IPPS réseau pour une imprimante qui annonce une interface USB **classe 7, sous-classe 1, protocole 4**. Il partage la pile hôte existante. Les imprimantes USB qui n’annoncent que PCL, PostScript, ESC/POS ou un protocole constructeur ne sont pas prises en charge par ce pilote.

La liste d’impression ajoute l’imprimante présente, avec un nom traduit et le suffixe USB. Le moteur interroge ses capacités et conserve ses exigences existantes : **PWG Raster couleur**, résolution et papier compatibles. Il envoie les requêtes IPP sur HTTP vers `/ipp/print` avec `Host: localhost` ; aucune conversion PCL/PostScript n’est ajoutée.

La tâche USB dispose d’une pile de 4 Kio et d’un transfert de 2 Kio. La réponse IPP est bornée à 64 Kio en PSRAM. Les en-têtes HTTP sont bornés à 8 Kio et les lignes à 512 octets. Le parseur consomme entièrement les réponses avec Content-Length ou chunked, y compris les trailers, et refuse les délimitations ambiguës. Une lecture a une échéance absolue. L’envoi utilise le flux raster existant sans copie complète supplémentaire.

Une annulation ou une réponse incomplète après le premier octet envoyé invalide le canal jusqu’à une reconnexion physique. Aucun travail n’est renvoyé automatiquement : une première transmission partielle pourrait déjà avoir été traitée par l’imprimante. La génération de connexion empêche également de poursuivre sur une autre imprimante branchée pendant le rendu.

## Sources techniques primaires

- [FatFS — f_mkfs](https://elm-chan.org/fsw/ff/doc/mkfs.html) : formats, tampon borné, partitionnement du volume physique et options de création.
- [FatFS — volumes et chemins](https://elm-chan.org/fsw/ff/doc/filename.html) : correspondance `VolToPart`.
- [Espressif — pilote USB MSC](https://github.com/espressif/esp-usb/blob/master/host/class/msc/usb_host_msc/README.md) : stockage BOT/SCSI ; l’implémentation utilise les interfaces de la version du SDK livrée avec le projet.
- [OpenPrinting — ipp-usb](https://github.com/OpenPrinting/ipp-usb) : protocole IPP-over-USB et nécessité de terminer les échanges HTTP pour éviter les réponses résiduelles.
- [OpenPrinting — manuel ipp-usb](https://github.com/OpenPrinting/ipp-usb/blob/master/ipp-usb.8.md) : conventions HTTP, dont `Host: localhost`, et différences de comportement entre périphériques.
