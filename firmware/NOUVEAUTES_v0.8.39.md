# Tab5OS 0.8.39 — Affichage, fichiers et périphériques

Cette révision part de la livraison 0.8.38. Le statut de compilation et les preuves actuelles sont consignés dans [VERIFICATION.md](VERIFICATION.md). Les priorités indiquées dans la demande classent la difficulté de réalisation, pas la valeur des fonctions. La distinction entre fonctions intégrées et travaux encore nécessaires est détaillée dans [EVOLUTIONS_0.8.39.md](EVOLUTIONS_0.8.39.md).

## Réglages et ouverture des fichiers

- **Rotation à 180°** dans les réglages d’affichage, mémorisée pour les démarrages suivants. L’orientation est aussi prise en compte par les lecteurs en portrait et lors du retour au paysage.
- **Polices par défaut distinctes pour l’interface et les documents** : sélection d’un fichier TrueType `.ttf`, validation avant enregistrement et application au prochain démarrage. Une police absente ou refusée conserve le recours à la police standard. Les blocs de police personnalisée sont limités à 6 Mio par fichier et 8 Mio cumulés ; le même fichier choisi pour les deux rôles est partagé. Les collections, polices variables et contours CFF ne sont pas acceptés par ce chargeur. La police de document personnalisée s’applique au style normal ; les variantes gras et italique conservent les polices fournies.
- **Ouvrir avec** propose six applications natives : Éditeur, Lecteur, Images, Dessin, Audio et Vidéo. Un choix ELF ou Python transmet le chemin du document à l’application sélectionnée. Celle-ci doit savoir lire cet argument et ce format ; le lanceur n’effectue pas de conversion implicite. Voir [OPEN_WITH_0.8.39.md](OPEN_WITH_0.8.39.md).
- **Dessin utilise `/Picture`** pour les nouveaux projets et exports. Les projets existants sous `/Images` restent accessibles et enregistrables. Aucun fichier personnel n’est déplacé automatiquement.

## Encodage et édition de code

L’éditeur garde du texte UTF-8 en mémoire et peut décoder/enregistrer UTF-8, UTF-8 avec signature BOM, UTF-16 little-endian, UTF-16 big-endian et Windows-1252. « ANSI » désigne ici explicitement **Windows-1252 occidental** ; il ne désigne pas toutes les pages de codes Windows. Une commande permet de rouvrir le fichier avec le choix d’encodage voulu. L’enregistrement refuse un caractère qui ne peut pas être représenté dans l’encodage choisi ; aucune substitution silencieuse n’est annoncée. UTF-32 n’est pas pris en charge.

La création de fichiers C et JSON est disponible, avec coloration adaptée et vérification syntaxique JSON bornée à 64 niveaux d’imbrication. La coloration C n’est pas une compilation. Pour l’impression d’un texte enregistré autrement qu’en UTF-8, l’éditeur demande d’enregistrer une copie UTF-8 : le pipeline d’impression ne traite pas directement les octets UTF-16 ou Windows-1252.

## Stockage et impression USB

Le formatage prend en charge **FAT12/16** sous le choix FAT et **FAT32**, selon la capacité du support. Il peut préparer un support dont le système de fichiers actuel n’est pas montable. L’opération vise le support USB entier, efface ses partitions et requiert une confirmation explicite. Les fichiers et applications qui l’utilisent doivent être fermés. L’annulation est possible avant le début des écritures. Une interruption ensuite peut laisser le support non montable. **exFAT et NTFS ne sont pas créés par cette version.**

L’impression ajoute **IPP-over-USB**, pour les périphériques USB annonçant l’interface correspondante (classe 7, sous-classe 1, protocole 4), tout en conservant IPP/IPPS par réseau. Cela ne vaut pas prise en charge universelle des imprimantes USB : une imprimante uniquement PCL, PostScript propriétaire, ESC/POS ou dépendante d’un pilote constructeur peut rester incompatible. Le transport USB réutilise le moteur IPP et exige la prise en charge du **PWG Raster couleur**, vérifiée avant l’envoi. Après une transmission USB interrompue, le branchement doit être renouvelé lorsque le pilote le demande.

## Meshtastic et développements suivants

L’interface Meshtastic utilise **Bluetooth pour se connecter à une radio Meshtastic externe**. La Tab5 n’acquiert pas de radio LoRa intégrée. Recherche des appareils, connexion/appairage, lecture des nœuds et échanges de messages texte sont fournis avec des limites de mémoire : 32 nœuds et 233 octets UTF-8 par message. Un message transmis à la radio n’est pas automatiquement un accusé de réception de son destinataire. Canal et confidentialité restent définis sur la radio.

Bitchat mesh, la compilation TCC sur le P4 avec gestion des dépendances et l’adaptation des outils matériels Flipper Zero restent des développements distincts. L’étude [EVOLUTIONS_0.8.39.md](EVOLUTIONS_0.8.39.md) précise les obstacles et étapes ; aucune interface factice n’est présentée comme une mise en œuvre de ces fonctions.

## Ressources et validation

Les nouveaux libellés sont traduits dans les quatorze langues. Les packs JSON ont été compactés en retirant seulement les espaces structurels, sans changer les traductions déjà livrées, pour conserver leur marge sous la limite mémoire. Les noms proposés pour de nouveaux fichiers et dossiers restent anglais. Le chargeur conserve désormais les traductions identiques au français dans les autres langues, au lieu de leur substituer à tort le secours anglais (par exemple « Liste » en allemand).

Firmware et ressources SD sont à mettre à jour ensemble selon [LISEZ_MOI.md](LISEZ_MOI.md). Les tests hôtes et la compilation sont consignés séparément des essais physiques : latence tactile, formatage effectif d’un support, compatibilité imprimante et échanges radio doivent être éprouvés sur la Tab5 et les périphériques réels. Aucun résultat matériel n’est déduit des seuls tests simulés.
