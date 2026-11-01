# Relecture indépendante de la persistance Média 0.8.47

## Résultat

Le module de production a été compilé et exécuté avec ASan/UBSan, à partir du fichier courant, puis comparé au module exact du ZIP 0.8.46. Les frontières remplacées sont NVS, création de tâche, allocation par capacités, horloge et compteurs DSI. Les workers utilisent de vrais threads hôte. Ni le pilote Flash ni FreeRTOS ne sont exécutés sur une Tab5 dans ce test.

Les deux implémentations conservent exactement le même record navigateur version 1 : **3 444 octets**, SHA-256 `773fa552b28ba58bf5277ff5fb5aada16f7dddd4c29ba20244002f644fe940b8` pour le scénario commun. Les noms NVS, le volume indépendant, les quatre vues, sélection, recherche, défilement et migration SD restent compatibles.

## Cas exécutés

- Migration initiale et tardive du volume, absence de carte, priorité du choix utilisateur, volume zéro.
- Debounce de 1,2 s et reprise de 5 s, y compris passage de `millis()` par zéro.
- 4 000 mises à jour inchangées sans écriture ni nouveau worker.
- Worker bloqué dans NVS : l'UI effectue 4 000 services, modifie le navigateur et conserve une demande de sauvegarde ; un thread modifie aussi le volume 50 000 fois. L'ancien instantané n'écrase pas les nouvelles valeurs.
- Refus de `begin()` pendant une écriture ; aucun état partagé détruit prématurément.
- Échec de chaque clé NVS séparément, échec d'ouverture du namespace, OOM de l'instantané, refus d'admission et échec de création de tâche.
- Réussite d'une clé préservée lorsque l'autre échoue ; nouvelle tentative limitée au composant restant à écrire.
- `flush()` ne renvoie pas de succès durable pendant l'attente ou l'échec. Le second appel après résultat effectivement écrit peut réussir.
- 400 cycles modification, commit et relecture après redémarrage simulé.
- Durée et compteurs DSI observés uniquement pendant un job ; absence d'observation DSI distinguée d'un compteur nul.

Résultats : **1 240 vérifications référence**, **5 271 vérifications courantes**. La référence exécute ses 808 tentatives d'écriture depuis le thread UI ; la version courante n'en exécute aucune depuis ce thread. Les totaux d'écritures des deux exécutions ne sont pas directement comparables, car la version courante reçoit en plus les scénarios concurrents et d'erreur propres à l'API asynchrone.

À la fin : zéro instantané de job alloué, zéro réservation conservée, aucun job en attente. Au maximum un instantané en vol. L'état navigateur courant reste volontairement résident ; le test ne prétend pas que toute la mémoire de l'application est libérée. La pile demandée est de 6 Kio, interne dans la vraie API `tab5CreateAdmittedTask` ; le test hôte n'en mesure pas le minimum matériel. LeakSanitizer est désactivé pour les contraintes du runtime ; les allocations de jobs et réservations sont suivies explicitement.

## Vérification de propriété et limites

`pending` appartient au thread UI. Le worker possède son instantané, publie le résultat avec un store release et ne touche plus l'instantané ensuite. Le thread UI acquiert cette publication avant la destruction. Le retour de `tab5CreateAdmittedTask` peut être précédé de l'exécution du worker, mais la publication de `pending` reste sûre : seul le thread UI consomme le résultat et l'instantané demeure alloué jusqu'à ce point.

Les champs de navigateur sont réservés à l'UI ; le volume est atomique. Une réussite de job met à jour seulement le volume et le hash effectivement enregistrés. L'état vivant reste comparé à ce hash lors du commit suivant. Aucun défaut de propriété ou perte de modification récente n'a été trouvé dans les chemins testés.

Cette refonte retire les opérations NVS synchrones du chemin graphique, mais **ne rend pas les transactions Flash du SDK exécutables sans suspension globale des caches**. Elle ne prouve donc pas la disparition du flash physique. Le diagnostic de corrélation permettra de vérifier cette piste sur appareil sans enregistrer de journal permanent.

Reproduction : `python3 tools/media-persistence-tests-0847/run.py --baseline /chemin/Tab5OS_0.8.46_Reseau_Memoire.zip`. Le JSON `tools/verification/media-persistence-0847.json` contient les empreintes exactes du module testé, les frontières et les limites. Le script échoue si les sources changent pendant l'exécution.
