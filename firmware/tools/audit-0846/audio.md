# Audio et vidéo — audit 0.8.46

Référence : archive livrée 0.8.45 (`46fd5da89ebb693a61d3ab34e2d3f3480a714aff76c095ce339f290ee8641573`). Le retour matériel ne valide pas la disparition des défauts radio, vidéo ou du claquement MP3. Les corrections précédentes de durée de vie HTTP et de changement d'horloge restent présentes ; elles ne constituent pas une preuve de disparition de tous les symptômes. Aucun essai physique, réseau C6 ou signal acoustique n'est exécuté ici.

## Causes établies et corrections ciblées

### Radio et Cartes : exclusivité conservée après le démarrage

`startRadioNow` conservait son `Tab5NetworkLease` exclusif et le drapeau `mediaPending` pendant toute la lecture. Cartes ne pouvait donc obtenir une transaction, même après que les allocations transitoires de démarrage radio étaient terminées. La propriété du transport et l'exclusivité de démarrage étaient confondues.

Après succès du décodeur et restitution de la réservation de démarrage, `shareAfterSetup()` conserve l'enregistrement de la radio vivante pour les transitions Wi-Fi, mais permet une transaction ordinaire sérialisée en parallèle. `mediaPending` est alors retiré. Le protocole du gate, ses bornes et ses tests sont décrits dans `network.md`. Les échecs gardent la fermeture complète par le propriétaire. Aucun socket vivant n'est libéré par Cartes et les contrôles de mémoire ne sont pas diminués.

**La pause radio de la 0.8.45 libérait déjà sa connexion.** Le test exécute `execTogglePause` et `cleanupDecoder` réels avec la source/producteur complets : le producteur est joint, la source détruite, le bail supprimé et `mediaPending` devient faux, tout en gardant la station et le mode Radio pour reprendre. Cinquante cycles avant/après le vérifient. Le blocage Cartes en pause provenait aussi de la condition d'interface qui assimilait la sélection du mode Radio à une lecture réseau active ; cette condition relève du correctif principal, pas d'une nouvelle fermeture de socket ici.

### Démarrage réseau : réveil à chaque tick sans travail

La tâche `radio_net` était créée avant le GET pour réserver réellement sa pile avant le premier afflux TCP. C'est pertinent pour la mémoire, mais elle attendait le transport par `while (!ready) vTaskDelay(1)`, pendant DNS, connexion, TLS et en-têtes. Elle pouvait ainsi se réveiller à chaque tick sans disposer de socket à lire.

Le départ utilise maintenant une notification FreeRTOS. L'ouverture réussie publie le transport puis notifie une seule fois. Si l'ouverture échoue, la fermeture publie l'abandon, notifie ce même départ une seule fois, puis attend le sémaphore de sortie. Après cette publication, le handle peut déjà être terminé : aucune notification tardive ne lui est envoyée. La notification de départ est consommée avant les notifications internes utilisées ensuite par StreamBuffer. Aucun nouvel objet permanent ou sémaphore n'est alloué.

### Ring PSRAM : utiliser les réveils du StreamBuffer

Le producteur faisait une écriture non bloquante puis un sommeil fixe de 20 ms quand le ring était plein. Le consommateur faisait une inspection puis dormait 5 ms quand il était vide. Des données ou de la place pouvaient arriver immédiatement après l'inspection, mais restaient inutilisées jusqu'à la fin du sommeil.

Les appels natifs `xStreamBufferSend` et `xStreamBufferReceive` attendent maintenant les données ou la place. Les 20/5 ms sont des bornes de réexamen de l'annulation, pas des délais imposés à une opération prête. Pour un ring plein, l'attente porte sur un octet, puis le producteur écrit la plage disponible : elle n'attend pas inutilement les 16 Kio d'un paquet entier. Les lectures courtes restent acceptées ; aucun seuil minimum de lecture n'est réintroduit. La limite totale de réception vide reste 8 s et la fermeture conserve la jonction avant libération.

### Lecture locale en pause : polling du décodeur inactif

`decoderRunning()` reste vrai lorsqu'un fichier local est en pause, car il faut préserver sa position. La boucle le classait néanmoins comme temps réel et se réveillait chaque tick alors que `serviceDecoder()` retournait immédiatement. Le classement tient désormais compte de `_paused`.

Hors décodage/enregistrement temps réel, `xQueuePeek` attend une commande sans la retirer ; son arrivée réveille immédiatement le propriétaire, qui traite la même file dans son ordre normal. Une limite de 20 ms conserve le service des transitions Wi-Fi et des demandes de priorité mémoire, qui n'utilisent pas cette file. La cadence de coopération du décodage actif reste présente. Ce changement ne suspend pas un enregistrement et ne supprime aucun choix d'enregistrement.

### Volume partagé entre interface et tâche audio

La revue indépendante a repéré deux lectures de `_volume` hors verrou dans `startLocalDecoder` et `startRadioNow`, tandis que l'interface l'écrit sous `StateGuard`. Même si ce champ fait un octet, l'accès concurrent sans synchronisation reste une course de données C++. Les deux réapplications du volume sont désormais entourées d'un `StateGuard` bref. La réapplication est conservée : une alarme peut avoir temporairement modifié le volume physique. Les ouvertures de fichiers, le bail réseau, l'admission, `claim`, le transport et les décodeurs restent hors de ce verrou.

`volume.py` compile les deux fonctions de démarrage et `setVolume` complètes, ainsi que le vrai `StateGuard`. Une frontière de champ instrumentée détecte les conversions du volume sans verrou ; les frontières de travail lent refusent un verrou conservé. Sur 300 démarrages réussis (quatre formats locaux et deux radios), puis un claim I2S refusé : 301 lectures hors garde dans la référence, zéro après ; le volume demandé est réappliqué après une modification physique simulée. Il s'agit d'un contrôle du contrat de synchronisation du code exécuté, pas d'un test acoustique ni d'une reproduction sur deux cœurs matériels.

## Structure mémoire et ownership examinés

| Ressource | Région / cycle actuel | Conclusion |
| --- | --- | --- |
| Tâche audio/décodeur | pile interne 16 Kio, priorité 5, cœur opposé à l'UI | admission avant création, réservation relâchée après allocation réelle, retraite par son propriétaire |
| Réception radio | pile interne 16 Kio, priorité 4, cœur UI | conservée avant connexion pour couvrir le pic ; attente de départ désormais passive |
| Speaker M5Unified | priorité 6, cœur audio ; pile 1280 + DMA length × 4 octets | pour 256 échantillons : 2304 octets ; DMA 256 × 8 × 4 = 8192 octets ; pas de diminution sans mesure |
| Ring radio | PSRAM, capacité adaptative 64 Kio à 1 Mio | un producteur/un consommateur ; destruction seulement après jonction |
| Buffer réseau radio | 16 Kio PSRAM | libéré après jonction du producteur |
| MP3 / AAC | arènes explicites PSRAM | libération après arrêt des générateurs ; le décodeur ne partage pas ses buffers avec LVGL |
| PCM intermédiaire | 3 × 2048 × 2 = 12288 octets PSRAM | M5 conserve des pointeurs ; `end()` joint son lecteur avant libération/réutilisation destructive |
| Commandes audio | 12 messages en PSRAM, contrôle FreeRTOS membre | `peek` ne consomme ni ne réordonne la file |
| Vidéo | pile interne 24 Kio, trois sorties JPEG en PSRAM | pas de libération d'image affichée ; buffers décodeur rendus après fermeture du flux |

Avec le répartiteur TLS PSRAM installé, `radioBudget()` demande 40 Kio internes (réception 16 Kio, Speaker/DMA et marge TCP), séparément de la pile audio déjà matérialisée. Aucune réservation de cette pile n'est gardée une deuxième fois pendant le démarrage radio. Sans ce répartiteur, le budget TLS interne est plus élevé, conformément aux allocations réelles prévues. Les réservations de sortie et de transport sont décrémentées après allocation, puis relâchées à la fin du démarrage. La faiblesse d'un bloc contigu interne ne peut pas être corrigée en prétendant disposer de PSRAM interchangeable.

Pas de nouvelle allocation permanente dans ces modifications audio. La frame statique GCC de `audioTaskEntry` passe de 48 à 320 octets (+272 pour le message observé par `peek`), dans la pile existante de 16 Kio. Ce chiffre est une frame de fonction, pas la profondeur cumulée de pile. La marge réelle doit être relevée sur l'appareil. Le plus grand bloc interne et la taille finale du firmware relèvent du bilan global.

## Inventaire des attentes restantes : ne pas confondre attente et blocage UI

| Emplacement | Attente restante | Justification / limite |
| --- | --- | --- |
| Source HTTP radio, corps et métadonnées ICY | 1 tick sans données, limite de lecture 5 s | worker réseau ; coopération et abandon observés ; pas de travail LVGL |
| Préchargement radio | inspection toutes les 20 ms, plafond 15 s | attend une quantité d'octets réelle ; une commande coupe l'attente ; cette boucle n'est pas prétendue événementielle |
| Producteur radio | pause 2 ms après lecture nulle encore connectée | évite un spin sur un serveur qui n'a rien produit ; sortie/abandon observés |
| Rafales radio | 1 tick après tranche d'environ 4 ms | laisse exécuter les tâches de moindre priorité, dont idle/watchdog ; `taskYIELD` seul ne suffit pas |
| Départ audio | 1 tick pendant publication initiale du handle | fenêtre courte `xTaskCreate`/publication ; pas l'attente HTTP longue supprimée |
| Décodage/enregistrement actif | 1 tick entre services | coopération de la tâche priorité 5 ; la sortie M5 applique aussi sa contre-pression |
| Priorité mémoire audio | état réexaminé toutes les 10 ms, timeout de l'appelant | exécuté depuis un worker ; fermeture uniquement par le propriétaire ; génération conservée |
| Fermeture radio | avertissement à 6 s puis jonction | ne jamais libérer une source encore utilisée ; un pilote réellement bloqué doit être diagnostiqué, pas transformé en use-after-free |
| Claim du pilote audio | mutex 10 ms et 1 tick, deadline de l'appelant | sérialise la propriété ; pas de reprise forcée d'un autre client |
| Submission PCM | `_play_raw` M5 attend son sémaphore natif ; retry extérieur borné | le `FLUSH_MAX_TRIES` extérieur ne borne pas un appel M5 qui serait bloqué dans le pilote ; limitation documentée |
| Parcours des catalogues | 1 tick toutes les 16 entrées | laisse la main, mais n'en fait pas un parcours asynchrone ; si appelé depuis UI, latence UI encore possible |
| Fin d'enregistrement | au plus 500 ms pour acquisition Mic, puis `Mic.end()` | worker audio ; conserve les buffers jusqu'à arrêt du lecteur matériel ; pas de sauvegarde ajoutée |
| Lecture vidéo réseau | 2 ms sans données ; deadline d'absence de progression 5 s | worker vidéo, annulation vérifiée ; pas de suppression de timeout |
| Parser vidéo | coopération 1 tick par tranche et avant décodage | évite la monopolisation par un flux MJPEG non cadencé ou invalide |
| PTS / pause vidéo | attente bornée par pas de 1–10 ms | synchronisation média et arrêt ; reste un polling, matériel non mesuré |
| Reconnexion MJPEG | backoff 0,5–5 s découpé en 20 ms | limite les tentatives et observe Stop ; n'est pas ajouté pour masquer les resets |

Les mutex de snapshot ne couvrent pas les handshakes HTTP. Leur attente indéfinie suppose toujours que le propriétaire termine sa section courte ; aucune preuve de deadlock spécifique à ces snapshots n'a été établie ici. Le timeout du wrapper ne remplace pas celui d'une opération DNS/TCP/TLS déjà engagée dans le SDK.

## Vérifications exécutées

- `tools/audio-tests-0846/transport.py` : classes radio complètes, véritables méthodes du SDK HTTPClient/Stream, source courante et archive 0.8.45. 200 cycles HTTP/HTTPS adaptés, 100 cycles producteur/consommateur, 50 pauses/reprises par les méthodes de production, échecs GET et départ interrompu, fermeture répétée, ICY et corps, réveils ring plein/vide. ASan et UBSan actifs. Aucune allocation tampon ni client suivi restant après les cycles.
- Le scénario HTTP maintenu ouvert avant démarrage établit des réveils de polling dans la référence et zéro après correction. Il ne mesure pas un gain CPU réel de la Tab5.
- `tools/audio-tests-0846/lifecycle.py` : méthodes réelles du cycle de vie, priorité et création de tâche, avec threads hôtes et frontières RTOS/heap/driver contrôlées. 400 cycles de retraite/recréation, commandes tardives, refus d'admission/création, priorité et commandes conservées. La lecture locale en pause ne fait plus de sommeil d'un tick ; la file devient son attente. Compteurs de piles et réservations suivies reviennent à zéro.
- Compilation objet avec les options réellement utilisées pour ESP32-P4 : `Tab5Audio.cpp`, `Tab5AudioDriver.cpp`, `Tab5Video.cpp` et leurs interfaces. La compilation complète et le lien final sont consignés au niveau de la livraison.

Les preuves sont `audio-transport-0846.json`, `audio-lifecycle-0846.json`, `audio-target-0846.json`, `audio-volume-0846.json`. Le relevé de référence du cycle de vie et les logs de tests sont dans ce dossier. Les frontières hôtes ne sont pas un scheduler FreeRTOS réel, un codec, un réseau ou un test de resampling. LeakSanitizer non utilisable ici ; ASan/UBSan et la comptabilité explicite restent distincts d'une preuve exhaustive de fuite nulle sur appareil.

## Problèmes restant ouverts

1. **Resets/échecs radio et vidéo sur l'appareil** : causes encore à établir avec raison du reset, dernière étape RTC `[NET][RESET]`, diagnostics HTTP/TLS, `[STACK]` et mémoire avant/après. Les correctifs prouvés n'autorisent pas à annoncer leur disparition.
2. **Claquement MP3** : la 0.8.45 avait retiré le redémarrage I2S supplémentaire lors du changement de fréquence, mais le retour matériel ne confirme pas la disparition du bruit. Le démarrage/arrêt codec et amplificateur reste celui de M5Unified. Aucun silence, délai, fondu ou reset supplémentaire n'est ajouté sans mesure.
3. **Flashs média/dessin** : pas attribués au seul lecteur audio. La dalle continue de scanner au repos. Voir l'audit affichage ; aucune modification spéculative de fréquence ou de reset matériel ici.
4. **Housekeeping Wi-Fi au repos** : `service()` peut créer puis retirer la tâche audio de 16 Kio toutes les 5 s pour RSSI et politique d'économie d'énergie. En mode Radio en pause, RSSI n'est pas relu, mais la politique de puissance reste servie. Supprimer ce travail sans remplacer son déclenchement ferait perdre les transitions d'économie d'énergie ; conserver une pile permanente augmenterait au contraire l'empreinte interne au repos. Ce point est identifié, pas présenté comme éliminé.
5. **RPC Wi-Fi répétés** : Cartes demande `WiFi.setSleep(false)` par transaction ; le SDK expédie le RPC même à état identique. Une centralisation doit inclure la politique de puissance audio et les transactions persistantes. Aucun changement tardif non validé dans cette livraison.
6. **Appel M5 bloqué matériellement** : la sortie conserve son propriétaire jusqu'à retour du worker SDK. Réinitialiser ou libérer les buffers après une attente arbitraire serait dangereux. Relever la tâche bloquée et la pile avant toute correction de ce chemin.

Test matériel court : radio seule → Cartes sans arrêter la radio → zooms pendant chargement → pause radio → nouveaux chargements et recherche → reprise ; contrôler son, tuiles, RSSI et aucune réinitialisation Wi-Fi. Puis MP3 → pause locale → reprise → vidéo distante → Stop pendant ouverture → retour radio. Avant/après chaque anomalie, conserver le log de démarrage et un relevé `d`, sans effacer SD, paramètres ou secrets.
