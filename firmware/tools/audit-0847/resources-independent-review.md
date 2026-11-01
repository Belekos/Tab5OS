# Revue indépendante — ressources et Écriture 0.8.47

Les six fichiers modifiés `Tab5ResourceManager.cpp/.h`, `Tab5Writing.cpp/.h` et `WriterSyntaxIndex.cpp/.h` ont été relus, ainsi que leurs appels d'initialisation, d'enregistrement de cache et de service dans le sketch. Aucun défaut bloquant supplémentaire n'a été identifié dans ce périmètre. Les empreintes des sources correspondent aux rapports `memory-0847.json` et `writing-cache-0847.json` au moment de cette revue.

## Synchronisation et propriété

- L'initialisation native est explicite avant `Tab5DebugSerial.begin()`, qui peut déjà demander une admission. L'appel ultérieur de `tab5ResourcesBegin()` retrouve les mêmes objets statiques ; il ne les recrée pas. Le stockage du mutex et du groupe d'événements appartient au gestionnaire global. Aucune allocation ou tâche supplémentaire n'est introduite par ces objets.
- Le mutex d'admission n'est conservé que pendant l'observation des quotas et du heap et la publication d'une réservation. `deficientDomains()` le libère avant un callback. Les callbacks de collecte peuvent donc libérer des ressources sans conserver le verrou global ; aucune inversion avec le verrou d'événement n'a été trouvée.
- `Building → Pending` publie budget et propriétaire. La collecte doit gagner `Pending → Processing` avant de les lire. Une expiration concurrente gagne soit `Pending → Free` avant ce gain, soit `Processing → Abandoned` après. Dans le second cas, le collecteur conserve l'emplacement jusqu'au retour du callback. Il n'y a pas de réécriture du budget pendant sa lecture.
- `Processing → Done` peut être suivi de `Done → Free` avant le `SetBits` associé. Ce signal ancien peut réveiller une nouvelle requête du même emplacement, mais l'état `Pending` de celle-ci n'est pas accepté comme une complétion. Le demandeur vérifie l'état à chaque retour d'attente. Le bit n'est qu'un mécanisme de réveil, pas l'autorité de publication.
- Les callbacks restent sur le collecteur UI, après mise à jour du propriétaire au premier plan et hors rendu. Le code de collecte ne rend pas les destructeurs LVGL exécutables depuis les workers. Les contrats d'appel du gestionnaire restent nécessaires ; `collectorThread=true` n'est pas une validation automatique de l'identité d'une tâche.

## Course de réutilisation exercée séparément

Un nouveau banc déterministe compile le véritable noyau Arduino du gestionnaire. La frontière FreeRTOS suspend le premier `SetBits` après publication de `Done`. Le premier demandeur expire, rend son emplacement puis réussit son admission finale. Une seconde demande réutilise cet emplacement et s'endort. L'ancien signal est alors libéré : il provoque un réveil suivi d'une nouvelle attente, sans admission ni callback supplémentaire. Seule la collecte propre à la seconde demande permet son admission.

Résultat ASan/UBSan : **32 vérifications, huit cycles de réutilisation tardive, réussite**. Source : `tools/memory-tests-0847/late_completion.cpp`. Reproduction : `tools/audit-0847/review_resource_race.py --work <répertoire_temporaire>`. Preuve : `tools/verification/resource-race-independent-0847.json`. Les adaptateurs utilisent des primitives de synchronisation hôtes ; ils ne mesurent ni les priorités ni la latence d'ordonnancement de l'ESP32-P4.

## Caches d'Écriture

`reclaimHiddenIndexes()` vérifie les états visible, modal, tableau et reprise avant de libérer les deux tableaux lexicaux. `SyntaxState` et ses checkpoints ne possèdent aucune ressource externe ; leur libération ne supprime pas les documents, sélections, textes enrichis ou historiques. `discard()` annule le pointeur et remet l'état de balayage à zéro. Une allocation de reconstruction échouée reste récupérable dans `SyntaxIndex`.

Le service syntaxique était déjà limité à la vue visible. Le nouveau garde du service structurel suspend sa progression quand Écriture est masquée, sans réinitialiser sa position. Les travaux d'enregistrement et de session demeurent servis avant ce garde ; la condition de sauvegarde automatique dépend toujours de l'option reçue. La reprise de la vue reconstruit la géométrie ou lance le rendu existant ; le service reprend ensuite l'indexation visible.

La décision de conserver les conteneurs structurels est justifiée par leur allocateur actuel, qui appelle `abort()` sur échec en compilation sans exceptions. Cette dette persiste et ne doit pas être présentée comme corrigée par l'expulsion des checkpoints lexicaux. Les 45 232 octets éventuellement libérables sont de la PSRAM, pas un gain de SRAM interne.

## Limites de la conclusion

Les attentes bornées de nettoyage de piles après collecte et la collecte synchrone depuis l'UI restent documentées dans `resources-lifecycle.md`. Cette revue n'affirme pas que toutes les admissions sont asynchrones, que tous les conteneurs récupèrent d'un OOM ou que l'OS n'a plus de fuite. Elle ne remplace ni la compilation complète finale ni les essais de coexistence, de mémoire et de fluidité sur Tab5.
