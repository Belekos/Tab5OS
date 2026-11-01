# Validation sur Tab5 — 0.8.44

Ces essais restent à faire : aucun matériel physique n'était accessible pendant la correction. Les tests sur ordinateur et la compilation sont consignés dans `VERIFICATION_0.8.44.md`.

Installer le BIN OTA par la procédure habituelle, conserver la table de partitions et les données. Ne pas formater la carte, effacer NVS ou réinitialiser les clés. Les ressources 0.8.44 sont fournies ; leur mise à jour doit conserver les personnalisations selon `LISEZ_MOI.md`. Enregistrer manuellement les documents de test si souhaité ; aucune autosauvegarde forcée n'est requise.

## Journaux utiles

Connexion série à 115200 bauds. Conserver le début du démarrage : version, cause du dernier reset, éventuel contexte réseau précédent et `[DISPLAY] double buffering: active/unavailable`. Noter la révision matérielle Tab5/dalle si connue.

- `m` : mémoire par capacités et état SD. Comparer notamment libre, minimum et plus grand bloc interne, puis PSRAM. Il s'agit d'un relevé ponctuel.
- `d` : compteurs d'affichage, DSI, sources d'invalidation et diagnostic UI, sans ouvrir une autre page ni redessiner l'écran.
- `p` : active/désactive le profilage UI optionnel. Ne l'activer que pour une courte reproduction de lenteur ; refaire `d`, puis `p` pour l'arrêter. Un échec d'allocation est indiqué, pas présenté comme un succès.

Éviter d'envoyer continuellement ces commandes : prendre seulement les relevés avant/après. Ne pas inclure mots de passe, clés ou URL contenant des tokens dans un retour de test.

## 1. Flashs sur accueil et Média

1. Après démarrage, laisser l'accueil sans interaction pendant trois minutes, avec extinction de l'écran désactivée temporairement si nécessaire. Envoyer `d` au début et après un flash ou à la fin ; noter son instant approximatif.
2. Ouvrir Média sans lancer de lecture. Refaire le même essai pendant une minute, puis pendant une minute de musique locale. Conserver les deux relevés de chaque phase.
3. Comparer les différences de `submissions`, `frames`, `areas`, `largeFrames`, erreurs et `underruns`, ainsi que `observed`/`active`.

Le scan DSI continue au repos : une augmentation de `scans` seule n'est pas un rafraîchissement demandé par l'OS. Les **underruns ne devraient pas augmenter** dans un fonctionnement sain. Si `observed=0`, leur absence n'est pas une mesure. Si le flash se produit sans nouvelle soumission/invalidations mais avec underrun, il faut investiguer le transport et la contention. Si aucun compteur ne change, conserver aussi la branche d'initialisation/dalle ; ce résultat ne prouve pas une cause matérielle à lui seul.

## 2. Musique → vidéo → retour

1. Lire un fichier audio local, puis une radio ; dans chaque cas, ouvrir une vidéo silencieuse MJPEG compatible. La musique doit être suspendue avant l'ouverture, puis reprendre selon la dernière intention utilisateur après fermeture.
2. Répéter avec une AVI MJPEG + PCM compatible. Vérifier son audio, pause, fermeture, retour et changement rapide de vidéo. Faire dix cycles.
3. Pendant une vidéo, demander Stop/changement de musique depuis les commandes disponibles, puis fermer : la dernière demande doit être respectée, sans reprise d'une ancienne sélection.
4. Pendant un enregistrement volontaire, ouvrir une vidéo : l'enregistrement doit continuer et la vidéo rester muette. Arrêter et enregistrer explicitement l'enregistrement si souhaité.

Relever `m` avant et après les cycles, erreurs de sortie et marges `[STACK]`. Ne pas utiliser un MP4 arbitraire comme test de conformité MJPEG.

## 3. Streaming interrompu

Avec une source MJPEG HTTP puis HTTPS compatible et connue fonctionnelle : démarrer, fermer pendant l'ouverture, recommencer, interrompre le serveur ou le Wi-Fi, puis rouvrir. La fermeture et l'erreur doivent rester récupérables. Une ouverture DNS/TCP/TLS engagée peut encore durer jusqu'au délai du pilote.

En cas de reset, conserver tout le journal jusqu'au redémarrage suivant : cause, backtrace éventuelle, contexte vidéo HTTP/TLS/streaming et admission mémoire. Indiquer protocole, format, étape (ouverture/lecture/arrêt), alimentation et durée, sans transmettre d'identifiants. La correction du use-after-free est vérifiée sur ordinateur ; seul ce journal pourra l'associer ou non à votre reset réel.

## 4. Cartes intensives → radio et SD

1. Envoyer `m`, ouvrir Cartes en ligne et faire une cinquantaine de déplacements/zooms, en laissant arriver des tuiles. Relever espace libre SD et éventuelles erreurs.
2. Revenir immédiatement à Média et lancer une radio ; envoyer `m` après sortie des cartes, puis après démarrage/échec de la radio. Répéter cinq fois, sans redémarrer entre les cycles.
3. Réouvrir l'Explorateur, vérifier qu'un petit nouveau fichier de test volontaire est lisible/enregistrable, sans écraser un document existant. Revenir à Cartes.
4. Désactiver le Wi-Fi volontairement : les tuiles déjà présentes et la recherche locale doivent rester utilisables. Réactiver et connecter : une absence de Wi-Fi ne doit pas être décidée à partir d'un ancien état audio. Refaire un changement pendant l'ouverture d'une radio.

Conserver les lignes `[SD]`, errno, HTTP/TLS et admission radio. `ENOSPC` indique l'espace, `ENOMEM` la mémoire, `EBUSY` une exclusion temporaire et `EBADF` un descripteur invalide ; ces erreurs ne doivent plus provoquer une récupération de bus. `EIO`/`ETIMEDOUT`/`ENODEV` restent de vraies erreurs à investiguer. Ne pas remplir artificiellement une carte contenant vos données pour reproduire une panne.

## Retour minimal

Fournir version 0.8.44, révision matérielle si connue, étape exacte, les deux relevés `m`/`d` pertinents et le démarrage suivant en cas de reset. Indiquer séparément si musique/vidéo, SD, mémoire et flashs se sont améliorés ou persistent : un seul succès ne valide pas les autres chemins.
