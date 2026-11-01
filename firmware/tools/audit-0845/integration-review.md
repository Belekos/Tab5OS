# Relecture et intégration SD — 0.8.45

Référence : archive livrée 0.8.44, immuable. Cet audit confronte les contrôleurs du sketch aux contrats réels de la bibliothèque FS compilée. Il ne présente pas les frontières de périphériques simulées comme une Tab5 exécutée.

## Chaîne de blocage établie

La bibliothèque Arduino retourne un `FileImplPtr` non nul même si l'ouverture échoue. C'est le booléen de l'implémentation et du `File` qui indique sa validité. L'ancienne enveloppe VolumeFS testait seulement la présence du pointeur, créait un `VolumeFile` et conservait une référence SD pour cette implémentation invalide.

Un membre persistant tel que `_offlineFile` de Cartes pouvait donc être faux tout en possédant encore cette référence. Son ancienne fermeture conditionnelle `if (_offlineFile)` la conservait. Dans le sketch, le contrôleur prenait bien son bail exclusif, mais refusait ensuite de démonter tant que `tab5SdOpenFiles()!=0`. Cette sécurité était correcte ; c'est la référence fantôme qui rendait sa condition de sortie impossible. Un redémarrage supprimait le membre et la référence.

La correction ne supprime pas la protection du remontage : VolumeFS ne compte plus une ouverture invalide et Cartes libère toujours la propriété de son membre, indépendamment de son booléen courant. Le contrôleur continue de refuser un remontage en présence de fichiers valides ou de travaux actifs.

**Exécution du vrai contrôleur :** avec les méthodes complètes `serviceSdRecovery`, `storageSdAction`, `probeSdWrite`, le vrai FS/VFS Arduino, VolumeFS et les baux de production, la 0.8.44 conserve une référence après l'ouverture manquée d'un pack. `serviceSdRecovery` n'atteint jamais `SD.end`. La correction revient à zéro référence et atteint la séquence de démontage/remontage et son probe. `SD.begin/end`, GPIO et SPI sont des adaptateurs contrôlés ; aucune reconnexion physique n'est revendiquée.

Les précédents tests de VolumeFS renvoyaient un `BasicFile` valide ou un pointeur nul. Cette frontière supprimait précisément le troisième état réel « pointeur présent, fichier invalide ». Un nombre élevé de cycles sur ce modèle ne pouvait donc pas trouver ce défaut. Le nouveau test exécute `FS.cpp`, `vfs_api.cpp`, leurs en-têtes et le contrôleur lui-même.

## Contrôleur et vérification d'écriture

Le test de reconnexion ignorait l'erreur de `flush`, la vérification de taille de sortie et l'échec de fermeture. Le marqueur Print `getWriteError` n'est pas un remplacement du résultat des fonctions VFS : les méthodes Arduino `File::write` et `File::flush` ne le renseignent pas automatiquement pour ces erreurs. Un fichier relisible depuis les caches pouvait donc conduire au message « lecture et écriture vérifiées » malgré l'erreur de synchronisation/fermeture.

Le probe conserve désormais la première erreur de l'ouverture, écriture, synchronisation, taille, fermeture, lecture de contrôle et nettoyage. Il n'effectue pas une nouvelle écriture si la vérification préalable d'absence du nom temporaire a échoué pour une cause inconnue. Il ne touche qu'au temporaire réservé qu'il a créé. L'erreur d'une opération de nettoyage ne masque pas l'erreur initiale.

La bibliothèque VFS est corrigée séparément par un backport ciblé : une erreur de `fflush` ne peut plus être écrasée par le `fsync` suivant. L'ordre et le nombre d'E/S restent inchangés. Le test actuel applique exactement ce backport dans sa copie de compilation isolée.

Dans douze scénarios combinant `fflush`, `fsync` ou `fclose` et EIO/ENOSPC/ENOMEM/EROFS, le contrôleur de référence annonçait à tort le succès du probe dans les douze cas. Le contrôleur corrigé ne l'annonce dans aucun de ces cas et conserve la cause injectée. Un cas supplémentaire vérifie l'erreur de `fflush` suivie d'un `fsync` qui modifie `errno` après succès.

## Préparation de l'exclusivité

La récupération automatique ferme maintenant le sélecteur de fichiers, qui peut garder un bail pendant toute la durée du dialogue, et annule explicitement l'itérateur « tout sélectionner ». Ces ressources de navigation n'effectuent pas d'écriture et leur fermeture ne déclenche aucun callback de sélection. L'éjection ferme aussi cet itérateur.

Ce point est distinct de la référence fantôme. La navigation normale vers Réglages appelle déjà `Tab5Tools.suspend`, lequel ferme le sélecteur ; ce sélecteur n'est donc pas présenté comme la cause démontrée du retour utilisateur « impossible depuis Réglages ». Il pouvait en revanche empêcher une récupération automatique tant qu'il restait ouvert.

Le marqueur de diagnostic « reconnexion SD active » n'est inscrit qu'après le dernier contrôle des références. Une attente avant démontage ne laisse plus un marqueur trompeur indiquant une opération de bus commencée.

La poursuite des tentatives après un probe échoué utilise désormais le même prédicat positif que le contrôleur central : EIO, ENODEV, ENXIO ou ETIMEDOUT. L'ancienne liste d'exclusions laissait notamment EEXIST et EINVAL provoquer un nouveau remontage, sans cause de transport. Les tests du contrôleur injectent ces deux erreurs au nettoyage et vérifient l'absence d'une deuxième opération de bus ; les quatre erreurs de transport conservent au contraire leurs reprises bornées. Cette vérification couvre donc aussi le risque d'arrêter toute récupération en corrigeant les reprises injustifiées.

Une seconde course a été reproduite lors de cette relecture : un worker refusé par l'exclusivité peut journaliser EBUSY après que le probe a journalisé son propre EIO. Lire ensuite `tab5StorageLastError()` utilisait le dernier événement d'une autre tâche et pouvait supprimer une reprise pourtant justifiée. Le probe retourne maintenant sa propre cause par référence, renseignée avant le journal ; la décision de reprise utilise ce résultat local, pas le diagnostic global. Le test crée réellement l'autre thread, fait refuser son ouverture par VolumeFS et vérifie que son EBUSY global n'écrase pas le résultat EIO du contrôleur. Le fragment exact antérieur est conservé séparément comme fixture de régression, identifié comme étape locale non livrée.


Les tests conservent les refus de démontage pendant un bail de worker, une lecture locale, un travail d'écriture, un traitement graphique ou une impression. Une demande manuelle reste asynchrone ; elle ne démonte pas la carte dans le callback. Les documents modifiés empêchent toujours l'éjection. Aucun arrêt forcé, autosauvegarde, délai ni nouvelle file n'est ajouté.

## Autres contrôles indépendants

- Les clients HTTP d'impression, des services Python, des archives Python et du transfert réseau déclarent leur transport avant le client HTTP ; la destruction inverse garde le transport valide pendant la destruction HTTP. Cartes conserve aussi cet ordre entre membres. Le chemin radio possédait encore le défaut de durée de vie déjà corrigé dans Vidéo ; il est traité par l'audit Audio, sans généraliser une modification de tous les verrous réseau.
- Le M5Unified réellement utilisé configure LCD_RST en entrée avec pull-up au démarrage. Les opérations de projet examinées lisent les mesures d'alimentation et modifient SPK_EN, pas LCD_RST. Aucune réinitialisation périodique explicite de la dalle n'a été trouvée ici. Cela ne disculpe pas le chemin DSI/cache ni une variante matérielle ; son audit reste distinct.
- `SD.begin` est synchrone : la source examinée ne justifie pas l'hypothèse d'un nombre de places de file insuffisant avant que le montage soit prêt. La référence fantôme et les erreurs d'E/S perdues sont des causes plus directement démontrées. Un défaut de synchronisation ou de pilote restant sur matériel doit être documenté séparément.

## Reproduction et limites

`python3 tools/sd-recovery-tests-0845/run.py --baseline-zip /chemin/Tab5OS_0.8.44_Medias_Cartes.zip`

Le test utilise le FS/VFS épinglé conservé dans `tools/storage-sdk-tests-0845/sdk`, le patch de compilation vérifié, les enveloppes de production et les fonctions exactes du sketch. Les opérations POSIX s'effectuent dans un répertoire temporaire ; leurs erreurs sont injectées au niveau de fflush/fsync/fclose. La couche String/Stream, mutex, USB, états des applications et périphériques est adaptée. ASan et UBSan sont actifs. LSan est indisponible ; les références et fermetures sont vérifiées explicitement.

Preuve actuelle : `tools/verification/sd-recovery-0845.json`, empreintes des méthodes, des sources complètes et des fixtures, journaux de compilation/exécution. Le test ne valide ni FatFs/SPI, ni la latence réelle, ni l'alimentation, ni la disparition de tous les incidents sur la tablette.
