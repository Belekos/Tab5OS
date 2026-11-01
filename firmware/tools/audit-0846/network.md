# Réseau et Cartes — audit 0.8.46

Référence : archive livrée 0.8.45, SHA-256 `46fd5da89ebb693a61d3ab34e2d3f3480a714aff76c095ce339f290ee8641573`. Le retour utilisateur indique que la SD ne décroche plus dans Cartes. Aucun essai physique de cette révision n’a été effectué ici.

## Causes établies dans le code

1. **Radio et Cartes s’excluaient pendant toute la lecture.** `startRadioNow()` conservait une `Tab5NetworkLease` exclusive, fondée sur un mutex FreeRTOS, jusqu’à la fermeture du lecteur. `mediaPending` restait également vrai pendant toute la lecture. Le worker Cartes attendait ce drapeau pour les tuiles et choisissait la recherche locale lorsque le transport était déclaré occupé. Cela ne correspondait pas à une perte réelle du Wi-Fi. Le test exécutant la source 0.8.45 reproduit le refus d’une requête Cartes alors que la radio est déjà établie.

2. **Course lors de la remise d’un transport conservé.** Un demandeur pouvait constater l’absence de connexion Cartes conservée, attendre le mutex pendant qu’une tuile établissait justement cette connexion, puis acquérir le mutex sans recontrôler son existence. L’ouverture radio pouvait alors chevaucher la mémoire TLS qu’elle voulait préalablement libérer. La source 0.8.45 reproduit cette admission prématurée ; le recontrôle 0.8.46 la refuse.

3. **Attente au mauvais endroit pour les recherches.** Le worker dormait sur la seule file de tuiles pendant 250 ms alors que les recherches étaient placées dans une autre file. Une nouvelle recherche ne le réveillait donc pas. Il se réveillait aussi quatre fois par seconde lorsque Cartes restait ouvert sans travail ni connexion à conserver.

4. **Temporisation fixe après les tuiles.** Chaque résultat de tuile encore courant ajoutait 60 ms, ou 220 ms en présence de la radio. L’API `setRadioPriority()` ignorait les valeurs de remplissage du tampon qui lui étaient transmises. Ce ralentissement n’était donc pas une régulation fondée sur les besoins réels du lecteur.

5. **Diagnostic de pause à ne pas surinterpréter.** Le chemin 0.8.45 de pause radio appelle déjà `cleanupDecoder()`, qui joint le récepteur, ferme le transport, supprime le bail réseau et remet `mediaPending` à zéro. Le symptôme utilisateur « indisponible même en pause » ne s’explique donc pas entièrement par le seul mutex conservé pendant la lecture. Les tests de commandes/pause de l’audit audio et la séparation des messages d’import Cartes par l’intégration principale complètent l’investigation. Une indisponibilité réelle restant après ces changements devra être confrontée aux erreurs HTTP/TLS et aux refus de mémoire sur l’appareil.

## Corrections et bornes de concurrence

`Tab5NetworkLease::shareAfterSetup()` est réservé à l’unique moteur audio par une méthode privée et une déclaration `friend`. Le moteur radio conserve son bail jusqu’à la fermeture mais, après l’établissement complet du transport, du tampon, du décodeur et de la sortie, il rend le mutex d’établissement. Il ne figure alors plus parmi les demandes qui obligent Cartes à céder. Le moteur audio remet aussi `mediaPending` à zéro à cet instant.

Il peut donc coexister **une radio établie et une opération réseau sérialisée**. On n’a pas rendu tous les téléchargements concurrents. La mémoire de démarrage reste contrôlée par les réservations existantes. La séquence radio reste : arrêter l’ancienne lecture, annoncer l’ouverture, faire terminer/fermer la transaction et le TLS conservé de Cartes, acquérir le bail, admettre le budget, allouer, puis permettre la coexistence. La course de remise du transport est recontrôlée après acquisition du mutex.

Le bail partagé reste compté par `tab5NetworkBusy()`. Une transition de pilote Wi-Fi attend à la fois la fin des opérations exclusives, la fermeture du transport Cartes conservé et celle de la radio. Détruire le bail radio partagé ne rend jamais le mutex désormais possédé par une autre tâche. Le lecteur en pause continue à fermer son transport ; le choix de station et sa reprise sont préservés.

Un groupe d’événements FreeRTOS statique signale la fermeture du transport conservé et de la radio. Il remplace la boucle `delay(1)` d’attente des connexions conservées. Les bornes de 20 ms servent à vérifier une annulation ou une désactivation qui n’a pas de signal propre ; les événements réveillent plus tôt quand la condition se réalise. Il n’y a pas de nouvelle tâche, de minuteur, d’allocation dynamique ou de réserve permanente de réseau.

Les deux files Cartes notifient désormais la tâche après un envoi réussi, sous le mutex de cycle de vie qui protège aussi sa sortie. Les changements de génération et la fermeture la réveillent également. La tâche attend une notification sans échéance lorsqu’elle n’a ni requête ni connexion conservée. Une notification envoyée avant l’attente reste mémorisée par FreeRTOS. Le faux élément de tuile utilisé auparavant pour réveiller une fermeture disparaît.

La seule vérification périodique conservée dans ce chemin d’attente est celle de la connexion HTTP conservée : au plus 250 ms entre contrôles durant sa durée de conservation maximale de 1,5 s, pour céder à une ouverture prioritaire et fermer la connexion expirée. Aucun sondage permanent n’est ajouté.

Le délai fixe de 60/220 ms et l’API qui ignorait le niveau du tampon radio sont supprimés. La file de résultat d’une seule tuile exerce déjà la contre-pression vers le consommateur graphique/SD. Il reste un seul worker Cartes, priorité 2 ; le récepteur radio, le décodage et le haut-parleur ont respectivement les priorités 4, 5 et 6. Cela établit des limites de concurrence, pas une preuve de qualité audio sur matériel.

## Délais examinés et conservés

| Chemin | Attente | Décision et justification |
|---|---|---|
| Réseau : fermeture du transport conservé | ancien `delay(1)` en boucle | Remplacé par événement FreeRTOS ; annulation vérifiée au plus toutes les 20 ms. |
| Cartes : aucune requête/connexion | ancienne lecture de file bloquante 250 ms | Remplacée par notification, sans réveil périodique ; les recherches et tuiles réveillent toutes deux. |
| Cartes : fin de tuile | ancien 60/220 ms | Supprimé ; file de résultat bornée et priorités déjà présentes. |
| Cartes : recherche Internet | minimum 1,1 s entre requêtes, contrôle par tranches 25 ms | Conservé : cadence du service de recherche ; sortie sur annulation/génération de recherche, pas délai de rafraîchissement graphique. |
| Cartes : import d’une région | intervalle propre à la source, vérification 20 ms | Conservé : respecte le débit choisi et les limites du serveur. |
| Cartes : résultat en attente d’interface | tranches 20 ms lorsque la file est pleine | Conservé : résultat possédé par le worker jusqu’à publication ou annulation ; pas de boucle quand il n’y a pas de résultat. |
| Cartes : établissement prioritaire d’un autre client | tranches 50 ms, réexamen de la recherche locale | Conservé : attendre une ouverture réelle en cours ; la radio déjà établie ne maintient plus cette condition. |
| Cartes : reprises HTTP | tranches 25 ms, nombre de tentatives borné par la politique existante | Conservé ; les imports s’arrêtent à la première erreur et ne relancent pas automatiquement les 403/429. |
| Lecture/écriture Cartes | `delay(1)` entre blocs SD de 32 Kio | Conservé : cession de CPU entre opérations de fichier ; ni file d’écriture infinie ni temporisation de montage. |
| Outils réseau : DNS/ping/ports | contrôles de cancellation 1/10 ms, délai d’écho | Inchangé : ces outils ont des échéances par opération ; pas de preuve que ces chemins causent les symptômes rapportés. |

**Coût restant identifié :** Cartes appelle `WiFi.setSleep(false)` par téléchargement et par recherche. Dans le SDK 3.3.9 effectivement utilisé, `WiFiGenericClass::setSleep()` effectue `esp_wifi_set_ps()` même si la valeur demandée n’a pas changé, donc une commande P4/C6 peut être répétée. La politique d’économie d’énergie du moteur audio possède actuellement son propre état. Ce point n’a pas été corrigé par une lecture concurrente non synchronisée du cache SDK ni par la suppression isolée du réveil du modem. Une correction ultérieure doit unifier ce propriétaire de politique et valider la sortie de veille quand le worker audio dort.

## Mémoire et CPU : ce qui est mesuré

Le nouvel état statique du garde réseau ajoute **48 octets** dans l’objet ESP32-P4 : groupe d’événements 40 octets, son handle 4 octets, compteur de radio partagée 4 octets. Le mutex existant est conservé. Aucun tampon, aucune pile, aucun worker et aucune limite de sécurité mémoire ne sont ajoutés ou réduits. La compilation de l’objet indique une plus grande trame locale de 80 octets dans le constructeur du bail ; cette valeur n’est pas une mesure de profondeur de pile cumulée.

Les tests de worker gardent le même maximum de mémoire PSRAM suivie par leur adaptateur, 13 568 octets. Ce n’est pas la mémoire PSRAM réelle consommée par Cartes sur la Tab5. Les transports HTTP/TLS réels, leurs allocations et le débit des accès PSRAM/SD doivent être mesurés physiquement.

## Validation exécutée

- `tools/network-gate-tests-0846/run.py` : source de garde 0.8.45 extraite de l’archive authentifiée et source 0.8.46 réellement compilées, frontières FreeRTOS adaptées à des threads/mutex/événements hôtes. ASan/UBSan actifs. Référence : 21 assertions, requête Cartes refusée pendant radio établie, admission prématurée du transport conservé reproduite. Courant : 1 835 assertions, admission radio+Cartes, course refusée, transitions Wi-Fi bloquées tant que la radio vit, annulation, désactivation pendant attente, propriétaires de mutex vérifiés. 100 cycles radio et 800 opérations concurrentes, une seule opération complémentaire à la fois, aucun bail restant. Les appels à `delay()` du garde passent de 24 dans cette exécution de référence à zéro dans l’exécution corrigée ; ce n’est pas une mesure CPU.
- `tools/maps-worker-tests-0846/run.py` : méthodes de production extraites sans modification (`setActive`, requêtes, worker, publication, lecture d’index, générations, libérations), vrai fichier d’index des villes, files/notifications/FS/HTTP adaptés à l’hôte. Référence : 666 assertions ; courant : 839. Les lectures bloquantes de la file de tuiles passent de 12 à zéro dans les scénarios testés. Attente sans travail, notification par recherche, fermeture, annulation, zoom pendant transfert, conservation du résultat possédé par l’interface, Internet avant local, repli local après échec/résultat vide et hors connexion testés. Aucun fichier, bail SD ou tampon suivi restant.
- `network-target-0846.json` : `Tab5NetworkGate.cpp` et `Tab5Map.cpp`, avec leurs en-têtes actuels, compilés avec le vrai SDK et le compilateur RISC-V P4 14.2.0. La compilation complète et la taille finale sont relevées séparément par l’intégration de livraison.
- Les tests radio/pause/fermeture et les prédicats d’import de l’interface sont consignés dans les rapports des modules audio et interface.

Les tests ne simulent pas une réussite sur la Tab5. Aucune mesure RF, de latence physique, de son, de scintillement, de consommation ou de perte SD n’est revendiquée. Les tests hôtes vérifient les causes logicielles et les responsabilités mémoire ; un essai radio + Cartes sur l’appareil reste indispensable.

## Essais matériels ciblés

1. Démarrer une radio et attendre la lecture effective ; ouvrir Cartes sur une zone non cachée puis lancer une recherche. Les tuiles et la recherche doivent utiliser Internet sans arrêter la radio. Vérifier absence de coupure et messages de refus mémoire/HTTP/TLS.
2. Passer la radio en pause, revenir dans Cartes, lancer l’import d’une région non cachée. Distinguer le message d’absence de Wi-Fi de celui d’une opération temporairement indisponible. La reprise doit rouvrir la station sélectionnée.
3. Pendant des tuiles en vol, zoomer plusieurs fois puis changer de station. La dernière vue doit rester celle affichée ; la nouvelle station doit être admise après la fermeture du transport Cartes conservé, puis le chargement peut reprendre.
4. Désactiver le Wi-Fi pendant radio + tuile, puis le réactiver. Aucune réinitialisation de carte, aucun blocage, aucun transport conservé ne doit empêcher une nouvelle lecture/recherche. Conserver les journaux d’erreur et les mesures mémoire avant/après.
