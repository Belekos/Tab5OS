# Contrat audio commun : corrections v0.8.49

Le pilote commun distingue maintenant **l’arrêt des références PCM** et **la fermeture du périphérique**. Les changements portent sur la propriété des ressources utilisée par toutes les applications ; aucun menu média n’est modifié par ce lot.

## Corrections

- `Tab5AudioDriver::stop()` demande l’arrêt au worker M5 et attend son acquittement. Il garde I²S, DMA, codec et amplificateur actifs pour la pause, le déplacement dans une piste et le remplacement immédiat d’une source.
- `release()` attend le mutex du propriétaire, joint le worker, ferme I²S puis libère les 12 Kio PCM et publie le propriétaire vide. Le retour silencieux après 250 ms a disparu. Les autres clients — vidéo, ELF, SDR et alarme — bénéficient du même contrat final.
- `cleanupDecoder(preserveTransport)` restitue toujours décodeur, workspace, source et bail réseau. Son option conserve seulement la sortie pour une transition ou une pause radio. Arrêt explicite, erreur de démarrage, fin d’album, interruption radio et transfert de priorité ferment la sortie.
- Une radio en pause ferme toujours son récepteur et son réseau. Elle conserve la sortie silencieuse ; lorsqu’une autre application demande la priorité, le propriétaire audio la restitue **avant** d’acquitter cette priorité, même en l’absence de décodeur actif. L’intention de pause est conservée.
- Le début FLAC vérifie le résultat de prise de sortie que sa bibliothèque ignorait. Une sortie occupée ne laisse plus ce décodeur se déclarer actif sans l’avoir obtenue.
- Un arrêt déjà acquitté n’émet pas un nouveau marqueur. La remise à zéro de la longueur et de la propriété suffit : les 12 Kio PCM ne sont plus réécrits inutilement à chaque arrêt. Seuls les échantillons effectivement initialisés sont soumis.

Le DMA, la fréquence de sortie, les affinités et les priorités restent inchangés dans ce lot. Aucun fade, délai analogique supplémentaire ou redémarrage d’essai n’est ajouté.

## Acquittement M5 et revue

La recette existante de préparation M5Unified ajoute `stopAndWait(uint8_t)`. Elle réutilise les slots atomiques, le worker et son sémaphore ; elle n’ajoute ni tâche, ni file, ni tampon permanent. Le consommateur remet l’interpolation à zéro lorsqu’il adopte le marqueur d’arrêt. L’attente se termine lorsque le bit d’activité **et les deux slots** sont vides.

La revue indépendante a relevé une fenêtre importante : à la fin naturelle d’un bloc, le worker peut éteindre son bit avant d’adopter un marqueur déjà publié. Attendre uniquement ce bit aurait pu rendre la main trop tôt, et le prochain producteur aurait alors remplacé le marqueur. La vérification des deux slots élimine cette fenêtre. Le test impose explicitement cet ordonnancement.

Le contrat suppose un canal exclusivement produit par Tab5. Aucun callback M5 de libération de buffers n’est installé dans ces sources. Il ne s’agit pas d’un arrêt universel de producteurs externes réentrants ; un callback qui republierait continuellement sur le même canal violerait cette exclusivité. Le worker normal n’acquiert pas le verrou de requêtes conservé par l’attente, ce qui évite une dépendance circulaire dans ce chemin.

Sur Tab5/ES8388, le chemin idle du worker émet des zéros et attend une notification ; il ne coupe pas I²S. La branche de coupure idle du DAC interne n’est pas utilisée. Le test surveille également les appels d’arrêt de l’horloge.

## Vérification exécutée

`tools/audio-contract-tests-0849/run.py` compile et exécute le vrai pilote Tab5, son adaptateur, les véritables fonctions de nettoyage/priorité du moteur et les véritables fonctions M5 de publication, démarrage, arrêt et mélange (`spk_task`). Les frontières FreeRTOS et I²S sont contrôlées par des threads hôtes.

Scénarios vérifiés sous AddressSanitizer et UndefinedBehaviorSanitizer :

- 100 cycles lecture, arrêt logique, reprise 44,1/48 kHz et fermeture finale ;
- sortie silencieuse après un signal non nul, sans résidu numérique du mélangeur ;
- arrêt demandé pendant que le worker est volontairement bloqué sur I²S ;
- fenêtre EOF avec marqueur publié avant extinction du bit d’activité ;
- restitution pendant une contention du mutex de 300 ms, sans publication prématurée du propriétaire vide ;
- Audio, Video, Native, Alarm et Sdr, refus de prise concurrente, refus d’allocation et de démarrage ;
- nettoyage effectif des sources/décodeurs/baux et garde d’échec FLAC ;
- radio pausée transférant la priorité à la vidéo, puis conservation de son intention de pause.

Les ressources suivies reviennent à zéro après fermeture. Cela vérifie les ressources de ce périmètre instrumenté, pas l’absence exhaustive de fuite sur le firmware physique. Le compte rendu et les empreintes d’entrée sont dans `audio-contract-tests.json`.

`target.py` compile séparément M5Unified, Tab5AudioDriver et Tab5Audio avec le SDK P4 réellement restauré et contrôles d’erreurs activés. Son résultat est `audio-target-tests.json`. La compilation complète et la liaison appartiennent au bilan général du projet.

## Ressources et limites

Pendant une pause chaude, la sortie garde ses 12 288 octets PCM PSRAM, ses 8 192 octets DMA nominaux et sa pile Speaker nominale de 2 304 octets, plus métadonnées/descripteurs du SDK. Ces valeurs sont des tailles configurées, pas une mesure du heap de l’appareil. À l’arrêt final elles sont restituées. Les gains de cette correction sont la suppression des cycles inutiles et un contrat mémoire sûr, pas une réduction artificielle de la mémoire d’une pause.

La radio pausée peut laisser sa tâche de décodage de 16 Kio se retirer. Une nouvelle demande de priorité peut nécessiter de la recréer pour fermer la sortie depuis son propriétaire. Si cette pile ne peut plus être allouée, la demande doit échouer proprement ; aucune fermeture I²S bloquante n’est déplacée vers l’UI pour masquer cette limite.

**Le claquement au tout premier démarrage analogique reste non certifié.** Conserver les horloges élimine des cycles pendant les transitions ; cela ne prouve pas l’absence de pop lors d’une vraie ouverture à froid. La séquence clocks → zéros DMA → codec/ampli reste celle de la livraison précédente. Essais nécessaires sur Tab5 : même MP3/FLAC, même volume, haut-parleur et casque, premier lancement puis changement, pause, seek et retour après vidéo/cartes. Ne pas annoncer la suppression physique des claquements à partir des tests hôtes.
