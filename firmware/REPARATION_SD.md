# Ressources système et vérification SD — 0.8.53

## Ressources de secours dans le firmware

Le firmware contient une copie des petits modules MicroPython officiels et des fichiers de configuration système initiaux. Le manifeste de la livraison et le générateur des ressources indiquent la liste et la taille effectives. Après le montage de la SD au démarrage, la récupération recrée les fichiers absents par petites étapes. Dans **Réglages → Stockage**, **Restaurer les fichiers manquants** relance l’opération et **Annuler** l’arrête. L'écran affiche l'avancement et le nombre de fichiers recréés.

Les fichiers sont écrits dans un temporaire, relus et publiés après vérification. Un fichier déjà présent, même vide ou personnalisé, n'est pas écrasé par cette récupération. Une erreur de support est signalée ; elle ne déclenche aucun formatage.

Ce secours ne contient pas les polices externes, les ELF, les archives Wikipédia, les documents ni les données personnelles. Les fonctions natives indispensables restent dans le firmware. Pour une ressource présente mais altérée ou pour les ressources officielles plus volumineuses, utiliser le contrôle Factory ci-dessous après avoir remis sa référence sur la SD.

Sans police externe chargeable à `/System/font.ttf`, l'interface utilise temporairement l'anglais et sa police intégrée. La langue préférée est conservée dans les réglages. Au prochain démarrage avec la police disponible, cette langue revient ; les documents ne sont pas modifiés.

## Audit et mise à jour conservatrice

L’outil contrôle les ressources de Tab5OS. Il ne répare pas la structure FAT et ne formate pas la carte.

L’audit est en lecture seule. Il exige la référence `Factory/Tab5OS-0.8.53`, les ressources attendues de son manifeste et leurs empreintes SHA-256. Cette version est définie par le module livré `MicroPython/lib/tab5factory.py` ; mettre à jour ce module et la référence ensemble selon [LISEZ_MOI.md](LISEZ_MOI.md). Une référence incomplète, corrompue ou d’une autre version interrompt l’audit avant toute modification. Ce contrôle est distinct de la restauration des fichiers absents depuis le firmware, qui ne dépend pas de Factory.

| Différence | Réparation normale |
|---|---|
| Fichier officiel absent | Installation depuis la référence |
| Ancien fichier officiel reconnu par son empreinte | Mise à jour avec sauvegarde |
| Script ou ressource modifiée par l’utilisateur | Conservé |
| Réglages JSON valides ou fichier INI existant | Conservés |
| Traductions personnalisées | Conservation des valeurs présentes ; ajout des clés manquantes |
| JSON invalide | Sauvegarde intégrale puis remplacement |
| JSON dépassant le budget de lecture | Conservé comme fichier modifié ; il n’est pas déclaré corrompu |
| Dossier à la place d’un fichier attendu | Signalé, jamais remplacé |
| Document, téléchargement ou autre fichier hors liste | Ignoré, jamais supprimé |
| Erreur EIO/5 ou autre problème de communication | Arrêt ; aucune suppression déduite de cette erreur |

Avant remplacement, une copie complète est créée sous `/.tab5-backups/repair/0.8.53/`, avec son chemin d’origine et un suffixe numérique distinct. Aucun nettoyage automatique de ces sauvegardes n’est effectué. L’option de restauration des fichiers modifiés est séparée et demande confirmation ; elle conserve également une sauvegarde.

Le plan est relu avant application ; un fichier changé entre aperçu et réparation entraîne l’arrêt. Les écritures passent par un journal privé. Une interruption pendant le renommage peut être annulée depuis ce journal. Des traces inconnues, des empreintes incohérentes ou une destination modifiée entraînent une demande de récupération manuelle : les données sont laissées en place. Un ancien journal ambigu ne doit pas être effacé pour contourner ce contrôle.

Ces protections ne rendent pas une carte défectueuse ni une coupure pendant une écriture FAT inoffensives. Les essais sur ordinateur injectent des erreurs de lecture et des interruptions de renommage ; les coupures électriques réelles n’ont pas été testées.

Ni la récupération depuis le firmware ni l'audit Factory ne restaurent un document supprimé, ne réparent la structure FAT ou ne formatent un support.
