# Affichage, entrées et latence — audit 0.8.42

## État initial vérifié

Référence : archive livrée `Tab5OS_0.8.41_Corrections.zip`, SHA-256 `01389322d10ae80b668ce369b2586e41831af03e3e407ecd3d60da62dd2667ff`. Les comparaisons extraient les sources de cette archive, sans reconstruire une version supposée. Dépendances : LVGL 9.6.0, M5GFX 0.2.30, Arduino-ESP32 M5Stack 3.3.9, ESP-IDF 5.5.4, cible P4 pré-révision 3.

La tâche Arduino `loopTask` (priorité 1) traite les entrées puis les services et `lv_timer_handler`. Le moteur LVGL, tactile et souris restent dans cette tâche ; la réception USB passe par `writer_usb` (priorité 3, pile demandée 4096 octets) et `writer_hid` (priorité 3, 6144 octets), puis une file FreeRTOS de 32 rapports souris. Ces tailles sont des demandes de création, pas des mesures de marge libre. Les points bloquants des autres applications sont traités dans les autres audits.

Le rendu utilise un tampon LVGL partiel de 80 Kio en PSRAM et deux images RGB565 de 720 × 1280 × 2 octets déjà possédées par M5GFX/IDF. Il ne comporte pas de troisième image. La synchronisation remet les régions précédemment modifiées dans le tampon arrière ; une première synchronisation complète existe à l'initialisation. Les writebacks sont découpés en 16 Kio maximum. Leur fonction IDF peut utiliser une section critique : absence d'interruption masquée non garantie.

Anomalies établies par lecture et exécution des sources :

- `finishPending(true)` attendait dans le flush LVGL, par `vTaskDelay(1)`, jusqu'à la libération du tampon ou 100 ms. Le traitement des entrées et des services partageant la tâche était suspendu pendant cette attente.
- Une rafale de 32 rapports de simple survol provoquait 32 lectures/distributions LVGL et appels applicatifs avant un seul passage de rendu.
- `lv_tick_inc(delta)` exécuté après les entrées ne mettait pas leur horloge à jour immédiatement et ne progressait pas pendant un callback bloquant. L'horloge native monotone corrige ce décalage ; elle ne rend pas les autres callbacks non bloquants.
- Sur débordement de la file USB, le producteur réinitialise toute la file et ne conserve que le dernier rapport. Deltas et transitions intermédiaires peuvent être perdus. Ce comportement existant reste une limite, explicitement non corrigée par le regroupement côté consommateur.

## Corrections

`Tab5DisplayPresent.cpp/.h` : vérification non bloquante de la barrière ; suivi de l'âge de publication et diagnostic de dépassement 100 ms, sans attente. La barrière conservatrice de deux callbacks est conservée. Aucune écriture dans l'image en cours de balayage n'est autorisée avant sa libération.

`Tab5DisplayRefresh.cpp/.h` : le callback du timer de rafraîchissement LVGL existant vérifie la disponibilité avant d'appeler `lv_display_refr_timer`. Sinon il suspend ce seul timer, conserve les régions invalidées dans LVGL et réarme le timer dès que la barrière est libérée. Les autres timers continuent. La vérification dans le callback est nécessaire : LVGL réactive ce timer lors d'une invalidation, y compris depuis un autre timer pendant `lv_timer_handler`. Un simple pause/resume placé avant le handler était insuffisant ; le test de rafale a détecté cette erreur avant livraison. Aucune tâche, file, image ou allocation permanente supplémentaire n'est créée par ce module (un pointeur et un booléen statiques).

`Tab5Mouse.cpp` : les positions de survol consécutives dans un lot borné sont distribuées une seule fois. Chaque delta, reste fractionnaire et limitation aux bords est toujours appliqué individuellement. Avant un clic, un déplacement avec bouton, une molette ou un relâchement, le survol précédent est livré à sa position ; toutes les transitions et tous les points de dessin restent livrés. Un rapport neutre isolé reste livré pour préserver les annulations applicatives lors d'un changement de portée.

Intégration principale relue : `lv_tick_set_cb` installé juste après `lv_init`, ancienne incrémentation supprimée ; `Tab5DisplayRefresh::service(lvDisplay)` appelé avant les deux appels existants à `lv_timer_handler`. Aucun appel de production à `lv_refr_now` n'a été trouvé ; un futur appel direct contournerait ce contrôle et doit être examiné.

## Comparaison reproductible

Rapport `tools/verification/display-latency-0842.json`. Sources .41/.42 exécutées avec LVGL réel, ASan/UBSan, doubles explicites pour le matériel. Modèle : événement DMA toutes les 16 ms, délai de boucle 8 ms, timer d'affichage 16 ms. Les nombres ci-dessous mesurent les attentes introduites par le code dans ce modèle ; ce ne sont pas des mesures de latence physique, de CPU ou de fréquence d'image.

| Mesure | 0.8.41 | 0.8.42 |
|---|---:|---:|
| Attente maximale du handler, changements continus | 24 ms | 0 ms |
| Intervalle maximal du service d'entrée, même scénario | 32 ms | 8 ms |
| Attente maximale du handler, DMA suspendu | 100 ms | 0 ms |
| Intervalle maximal du service d'entrée, DMA suspendu | 108 ms | 8 ms |
| Retard maximal de l'horloge LVGL pendant cette attente | 100 ms | 0 ms |
| Distributions pour 32 rapports de survol | 32 | 1 |
| Points livrés pour un tracé de 20 rapports avec bouton | 20 | 20 |
| Trames ignorées dans le scénario combiné | 16 | 0 |

La reprise publie la dernière région invalidée même sans nouvelle interaction. Un timer LVGL indépendant invalide aussi pendant le handler pour vérifier ce chemin. La barrière de deux callbacks limite encore la cadence de réutilisation des images ; aucun gain de fréquence d'image physique n'est revendiqué.

## Validation exécutée

- Tests de framebuffer/cache, ownership, erreurs de publication/writeback, wrap de compteur, rafales, arrêt DMA et reprise, avec LVGL réel sous ASan/UBSan : rapport `tools/verification/display-0842.json` et journaux associés.
- Modèle séparant octets cache CPU, PSRAM externe et image balayée : les écritures et writebacks vérifient la conservation des octets visibles. 180 secondes simulées de mises à jour de libellé masqué ne produisent aucune publication ni writeback supplémentaire. Ce test ne couvre pas tout l'OS ni le débit physique de la PSRAM.
- 9 092 assertions souris/cartes existantes rejouées : gaucher, boutons combinés, menus contextuels, molette, débranchement, parcours de saisie/dessin et zoom. Rapport `tools/verification/maps-input-0842.json`.
- Comparaison identique avant/après : `tools/display-tests-0842/compare.py` ; tests de survol, 20 points de tracé, fraction de vitesse, rebond aux bords, budget 32 rapports, relâchement, paquet neutre et débranchement.
- Compilation des trois unités modifiées/nouvelles avec le compilateur P4 et les vrais headers SDK : `tools/verification/display-modules-target-0842.json`, réussite. La compilation complète et les tailles finales figurent dans le bilan général.

## Problèmes résiduels et matériel

**Élevé, établi :** débordement producteur de la file USB. Le retrait des attentes d'affichage réduit une cause de pression mais ne garantit pas l'absence de saturation par d'autres opérations bloquantes. Une évolution doit préserver l'ordre des boutons et des chemins de dessin ; augmenter arbitrairement la file ou agréger les mouvements à travers les clics ne constitue pas une correction démontrée.

**Élevé, cause non établie :** flashs turquoise. Le modèle ne reproduit pas un transfert physique MIPI-DSI ni la contention du bus PSRAM. Aucun reset périodique n'a été ajouté. L'hypothèse d'underrun/bande passante doit être confrontée aux compteurs DSI et aux horodatages de grandes invalidations. Ni LVGL ni le panneau ne sont déclarés responsables sans ces observations.

**Moyen :** des opérations synchrones d'autres services peuvent toujours retarder la boucle. Le nouvel instrument de latence de la boucle permet de les distinguer de l'affichage. La suppression de l'attente dans ce module ne prétend pas éliminer toute source de lag.

Procédure courte Tab5 : activer le profilage, laisser l'accueil immobile 3 minutes, relever le bilan, puis déplacer rapidement la souris, tracer des boucles en maintenant le bouton et effectuer des clics pendant radio/vidéo/cartes. Relever les grands intervalles de boucle, phases lentes, compteurs de timeout/publish/cache/DSI, heure exacte et contexte de chaque flash ; vérifier que le dernier état visuel se met à jour après une pause. Désactiver le profilage ensuite. Aucun essai sur Tab5 physique n'a été effectué ici.
