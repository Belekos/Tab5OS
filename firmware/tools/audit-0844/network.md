# Réseau partagé — audit et corrections 0.8.44

## Constats établis sur les sources livrées 0.8.43

- Les transports cartographiques `NetworkClient` / `Tab5SecureClient` n'utilisaient pas l'adaptateur coopératif déjà employé pour les transferts du terminal. L'annulation n'était vérifiée qu'entre certaines opérations HTTP et lors de l'écriture du corps reçu. Un serveur laissant un en-tête incomplet ou un corps sans nouveaux octets pouvait donc retenir le worker et ses ressources jusqu'au délai du transport. Le changement de page ne constituait pas une annulation effective de cette attente. La modification cartographique est détaillée dans `maps.md`.
- Le verrou réseau sérialise les sessions complètes, avec priorité coopérative aux clients de premier plan. La radio prend ce verrou avant son admission mémoire, et attend la libération des sockets cartographiques retenus. Ce verrou ne garantit pas à lui seul que la pile de la tâche Cartes a déjà été restituée. Aucun défaut de propriété ou de comptage de ce verrou n'a été reproduit dans les scénarios contrôlés ; ses sources sont conservées.
- Le cache d'état `WiFi.status()` est lu dans le SDK installé ; ce n'est pas une interrogation RPC du C6. Un Wi-Fi activé et une station associée restent deux états distincts. Aucune défaillance ESP-hosted ni perte réelle d'association n'a été reproduite sans Tab5. L'interface cartographique est traitée séparément dans les corrections principales.
- Le transfert du terminal déclare les deux transports avant son objet `HTTPClient`, dont le destructeur passe donc avant ceux des transports. `Tab5MapConnection` possède également ses transports avant son `HTTPClient`. Ces deux modules ne présentent pas l'ordre de destruction invalide confirmé dans la vidéo. Aucun changement arbitraire de la bibliothèque HTTP ou du protocole P4–C6 n'est ajouté.

## Modifications

`Tab5CooperativeClient.h` expose deux opérations sur la tâche propriétaire : `setCancellation(callback, contexte)` lie une nouvelle requête, `clearCancellation()` enlève le pointeur avant la destruction de son contexte. Le drapeau d'interruption reste actif après le nettoyage et n'est remis à zéro qu'au début de la requête suivante. Aucun champ, objet dynamique, tâche ou minuteur n'est ajouté. Le moteur cartographique réutilise cet adaptateur avec un nettoyage de portée ; les appels HTTP et les callbacks restent sur son worker. La vérification intervient dans les lectures et les attentes d'octets. Une connexion DNS/TCP ou une poignée de main TLS déjà engagée conserve les limites temporelles du pilote ; cette correction ne prétend pas les interrompre depuis une autre tâche.

La remise à zéro du délai Stream à l'annulation est vérifiée avec les fonctions du SDK réel : `HTTPClient::setTimeout()` ne modifie pas le transport déconnecté, mais `HTTPClient::connect()` restaure systématiquement `_tcpTimeout` après la connexion. Le prochain en-tête fragmenté est donc lu avec le délai configuré, et non zéro. Les méthodes de connexion reçoivent également le délai de connexion explicite.

`Tab5NetDiagnostics.h/.cpp` ajoute les états « connexion HTTP/TLS vidéo » et « lecture flux vidéo » au mot de bits RTC existant. Le prochain démarrage peut identifier un reset au cours de ces opérations. L'arrêt vidéo n'efface pas les contextes radio/cartes. Le record RTC reste de **8 octets**, sans écriture Flash, sans journal de contenu/URL/secret et sans échantillonnage périodique ajouté. L'intégration vidéo est décrite dans `video.md`.

## Vérifications exécutées

`python3 tools/network-tests-0844/run.py --build <dossier-temporaire> --sdk-core <installation-Arduino-ESP32>`

- Source réelle de l'adaptateur et du garde de portée cartographique, 1 000 cycles annulation/reprise plus 1 000 cycles de liaison des deux transports Cartes, absence d'octets après annulation, callback nettoyé avant libération réelle de son contexte, nouvel usage de la même connexion, temps monotone avec débordement 32 bits et passage de la main au planificateur : réussite sous ASan/UBSan.
- Source réelle du journal RTC : transitions vidéo/cartes/radio indépendantes, reprise du record lors d'un redémarrage simulé et 30 000 cycles concurrents avec les opérations atomiques de production : réussite. Taille du record inchangée à 8 octets.
- Source réelle du verrou réseau et fixture historique : refus hors service, désactivation pendant l'attente et pendant l'acquisition, annulation, ressources d'arrière-plan retenues, absence de libération du mutex d'un autre propriétaire, réactivation : réussite.
- Fonctions `HTTPClient::connected`, `setTimeout`, `connect` et `Stream::timedRead` extraites du SDK effectivement compilé : après annulation, reconnexion puis réception fragmentée d'un en-tête : réussite. Les fonctions extraites et les SHA-256 des sources du SDK sont dans `tools/verification/network-0844.json`.
- Compilation objet ESP32-P4 des modules réseau HTTP, verrou réseau et diagnostic réseau : réussite dans `tools/verification/network-target-0844.json`.

Les transports, le temps, les frontières RTOS et la radio sont contrôlés dans les tests hôtes. Ces tests ne représentent pas des échanges réels TLS/Wi-Fi ni une exécution sur FreeRTOS physique. LeakSanitizer n'est pas exploitable dans cet environnement (`/proc` indisponible lors de son inspection des tâches) ; il est désactivé explicitement, ASan et UBSan restent actifs. Aucune mesure matérielle de latence ou de mémoire réseau n'est annoncée.

## À confirmer sur Tab5

Reproduire un flux interrompu et la transition Cartes → Radio, puis fournir les événements `[NET][RESET]`, le motif de redémarrage et les lignes `[MAP][NET]`, `[WIFI]` et `[HOSTED]` déjà disponibles. Les logs ne doivent contenir ni mots de passe, ni PIN, ni clés. Les adresses privées affichées par le diagnostic réseau existant peuvent être masquées avant partage.
