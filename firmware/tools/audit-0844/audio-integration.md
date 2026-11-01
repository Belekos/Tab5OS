# Audio et intégration réseau — 0.8.44

Référence immuable : ZIP livré 0.8.43, SHA-256 `a0c58c0ba1003722e3f834eac0618dc937c5395afb7cc650dd03220730595e52`. Aucun essai sur Tab5 physique.

## Observateur du lecteur

Le moteur publie déjà ses instantanés au plus toutes les 50 ms. L'ancien indicateur du bandeau avait sa propre révision, mais le lecteur dépendait d'un balayage de 250 ms pour découvrir progression, durée et niveau d'enregistrement. Son drapeau historique ne couvrait pas ces champs.

`mediaSnapshot` ajoute une révision indépendante pour les champs affichés : mode, pause, volume, titre/identités, lecture effective, statut, progression, durée estimée/écoulée et enregistrement. Le niveau ne déclenche un événement que pendant l'enregistrement. Une révision inchangée ne prend aucun verrou et ne copie aucune chaîne. Une révision modifiée est copiée sous verrou sans attente ; une erreur de copie de l'une des six chaînes conserve la révision pour réessayer. L'appelant ne présente pas une copie échouée, y compris lors d'un événement vidéo.

Les changements de RSSI ou de remplissage réseau restent consultables sans actualiser le lecteur. `snapshot`, `takeUiDirty`, l'observateur du bandeau et leurs contrats publics restent disponibles. Le lecteur regroupe les progressions avec l'intervalle existant de 250 ms ; aucun rendu à 20 Hz n'est introduit. Les actions forcées et notifications vidéo restent immédiates. Une modification moteur ordinaire peut être présentée jusqu'à 250 ms plus tard.

Test des méthodes exactes avec adaptateurs String/mutex/périphériques, ASan/UBSan : sur 60 secondes contrôlées au repos, le lecteur de référence demande 240 instantanés, soit 1 440 copies de String ; le nouvel observateur interrogé toutes les 8 ms n'effectue ni copie ni prise du verrou par le lecteur. Ce sont des copies logiques, pas une mesure d'allocations physiques. Les six erreurs de copie, contention, observateurs concurrents, retour du compteur, progression, horloge radio et enregistrement sont vérifiés. La publication périodique du moteur n'est pas supprimée.

## Bail vidéo depuis l'audio inactif

La vidéo obtient maintenant un bail audio même lorsqu'elle n'a pas de PCM. Pour ne pas transformer une vidéo silencieuse auparavant fonctionnelle en demande inutile de pile interne, `serviceIdleMemoryPriority` reconnaît un audio complètement inactif : worker arrêté, mode Idle, aucun décodeur/source/fichier ni reprise différée. Le service UI peut alors acquitter et terminer uniquement le protocole atomique du bail, sans créer un worker de 16 Kio, sans traiter de commande et sans accès fichier/réseau. Une tâche audio active conserve la propriété exclusive de ses ressources et la suspension/reprise normale.

Le bail reste tenu pendant la vidéo. Les commandes utilisateur ne sont pas perdues ; un worker nécessaire continue de les traiter selon le protocole existant. Une pénurie mémoire conserve la commande dans la file. Les travaux optionnels de RSSI n'allouent pas une nouvelle pile pendant ce bail inactif.

Test des méthodes réelles `audioTaskEntry`, `canRetireAudioTask`, `reclaimIdleWorker`, `startAudioTask`, `service`, `serviceIdleMemoryPriority`, `acquireMemoryPriority` et du trampoline/contrôle atomique réels, avec threads hôte et frontières simulées : 400 cycles, publication avant retour de création, échecs d'admission/création, commandes proches du départ du worker, audio/enregistrement actifs, bail inactif malgré admission refusée, générations périmées. Aucun octet de pile simulée ni réservation ne reste après les cycles. Le nombre d'assertions varie avec l'ordonnancement hôte ; ce test ne mesure ni la pile réelle ni le scheduler FreeRTOS.

## État réseau des cartes

Les décisions de connexion des cartes utilisaient l'instantané publié par l'audio. Celui-ci peut être ancien pendant une ouverture/fermeture de flux. `mapNetworkConnected` lit désormais l'état de connexion mis en cache par les événements du SDK, ainsi que l'autorisation réseau de l'OS, sans RPC C6. La politique audio utilise un instantané scalaire au lieu de recopier six chaînes. Wi-Fi activé n'est toujours pas assimilé à Wi-Fi connecté.

Les 33 contrôles de prédicats réels couvrent les modes audio, l'autorisation réseau, les états SDK et des instantanés audio volontairement décalés dans les deux sens. Sur 1 000 appels, 6 000 copies de chaînes deviennent zéro. La disponibilité effective du point d'accès et les transitions du C6 restent à tester sur matériel.

## Diagnostic des flashs

La commande série existante `d` imprime maintenant aussi les compteurs Present/DSI/LVGL et leurs sources d'invalidation. Elle ne touche pas l'écran, ne réinitialise pas le pilote et n'ajoute aucun timer ni écriture persistante. Un relevé avant/après permet de distinguer une publication LVGL, un underrun et un flash sans changement de ces compteurs. L'absence d'observation DSI est indiquée explicitement.

## Preuves

- `tools/audio-media-tests-0844/run.py`, `tools/verification/audio-media-0844.json`.
- `tools/audio-lifecycle-tests-0844/run.py`, `tools/verification/audio-lifecycle-0844.json`.
- `tools/map-routing-tests-0844/run.py`, `tools/verification/map-routing-0844.json`.
- `tools/verification/audio-target-0844.json`, compilation complète pour l'intégration du sketch.

ASan et UBSan actifs ; LSan désactivé car non exploitable dans cet environnement. Les frontières de fichiers, décodeurs, clients et RTOS ne sont pas présentées comme du matériel exécuté.
