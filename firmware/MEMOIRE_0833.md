# Gestion mémoire — 0.8.33

La 0.8.33 supprime les purges périodiques introduites dans les versions précédentes. Le temps écoulé, une navigation ou une mesure de mémoire faible ne déclenchent plus une collecte générale. L’OS intervient lorsqu’une opération demande effectivement un budget qu’il ne peut pas encore lui accorder.

## Défauts corrigés

Les sources livrées en 0.8.32 ont été exécutées dans cinq scénarios contrôlés. Elles libéraient un cache après 30 secondes sans besoin nouveau ; elles tentaient une récupération périodique avec 46 Kio de SRAM disponibles alors qu’aucune application ne démarrait ; elles pouvaient atteindre les caches de l’application visible et de l’opération demandeuse ; enfin, un échec d’allocation PSRAM déjà traité par l’appelant pouvait provoquer ensuite la destruction d’un cache SRAM sans rapport avec cet échec.

Ces cinq comportements ne se produisent plus avec le gestionnaire corrigé. Cette reproduction établit les défauts logiciels, sans attribuer avec certitude chaque flash observé sur tablette à l’un d’eux. Les modifications d’affichage et leur validation sont décrites séparément dans le bilan de la version.

## Admission liée à une opération

1. L’appelant indique son propriétaire et ses besoins : octets de SRAM, plus grand bloc interne, bloc DMA, octets de PSRAM et plus grand bloc PSRAM.
2. Si ces besoins sont déjà satisfaits, l’OS accorde la réservation sans appeler de destructeur de cache.
3. Sinon, il tente de restituer uniquement des caches d’arrière-plan capables de libérer le domaine réellement déficient, en commençant par les moins récemment utilisés.
4. Le propriétaire visible et celui de la demande restent exclus de cette collecte. Les callbacks des applications protègent également les lectures, impressions et traitements encore actifs en arrière-plan.
5. L’admission est réévaluée sur les quantités réellement libres. Un échec conserve l’état actif et retourne un diagnostic ; il ne déclenche pas une nouvelle purge anonyme plus tard.

Les demandes provenant d’un worker sont traitées par le thread de l’interface, qui possède les objets à libérer. L’attente est bornée et coopérative. Son délai limite l’attente du worker : il ne programme aucune expiration de cache. La pile d’une tâche terminée peut être rendue ultérieurement par FreeRTOS ; une notification de terminaison n’est donc pas comptée comme de la mémoire déjà disponible.

Les réserves de sécurité restent de **32 Kio de SRAM et 512 Kio de PSRAM**. La PSRAM ne remplace pas les blocs internes ou DMA nécessaires à certains pilotes et piles. Les réservations ne doublent pas les allocations déjà réalisées : `allocated()` retire du budget futur les octets que le tas compte désormais comme utilisés. `returned()` précède la restitution d’un bloc si son quota doit rester disponible pour l’opération.

## Durée de vie des données

Les ressources temporaires sont libérées à une étape définie : fin, annulation ou fermeture de leur opération. Les caches rechargeables peuvent rester disponibles tant qu’une nouvelle demande n’exige pas leur récupération. Le passage du temps ne constitue plus une étape de fin de vie.

Les documents, modifications non enregistrées et historiques d’édition appartiennent à leur application. La collecte générale ne les détruit pas. Atelier Image possède son propre propriétaire mémoire ; sa mise en arrière-plan conserve le document et son historique, tout en libérant explicitement son aperçu dérivé, qui sera reconstruit à la reprise. Le changement de page actualise le propriétaire visible avant la préparation du nouvel écran. Les callbacks de Cartes, de l’explorateur et des médias gardent aussi une protection de leur page sous un panneau d’outils.

Les échecs de bas niveau de `malloc` restent journalisés. Ils ne déclenchent aucune collecte différée : l’appelant peut déjà avoir réduit une allocation facultative, récupéré autrement ou annulé. Une opération qui doit récupérer de la mémoire passe par `reserveRecovering()` avec son budget encore pertinent.
