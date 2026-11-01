# Revue indépendante du worker catalogue — 0.8.43

Revue statique de propriété, synchronisation et admission, distincte des tests exécutés par le lot catalogue. Sources examinées et empreintes finales :

- `Tab5AppCatalogue.cpp` : `ae1a41f49afd46b4462b0efd200c2921f1a6b5ca9a72dbb30dce4328409b26bc`
- `Tab5AppCatalogue.h` : `9640049a2db0f7c5c6f9d547786ab78d61c546e2026c6497a00d42ee31ac6efd`
- Contrats de tâche/admission : `Tab5Task.h`, `Tab5ResourceManager.h/.cpp` ; handles et baux : `Tab5Storage.h`.

| Transition examinée | Vérification |
|---|---|
| Création | Le worker ne démarre son vrai travail qu’après publication du wrapper de tâche et restitution du quota. `workerRunning` est déjà vrai. Une admission directe évite les 60 ms de récupération UI pour ce travail optionnel. |
| Écriture du scan | Le worker écrit uniquement le staging et son état d’E/S. `addBuiltin` reste dirigé vers la table publiée UI ; il n’utilise plus l’état global du scan pour choisir implicitement son propriétaire. |
| Publication | Le worker ferme ses handles/baux, exécute son checkpoint puis publie son résultat et `workerRunning=false` en release. L’UI lit le booléen en acquire avant de consulter les champs non atomiques et d’échanger les tables. |
| Annulation | `endScan()` pose la demande et ne ferme/réinitialise pas les E/S ou le staging encore possédés par le worker. Il n’attend ni ne tue la tâche. |
| Nouvelle demande pendant annulation | La demande est regroupée côté UI ; la réinitialisation du staging et les nouveaux paramètres n’ont lieu qu’après l’acquittement de fin. |
| Reset pendant scan | La table publiée et ses compteurs UI sont distincts du staging. Le worker ne lit pas la personnalisation NVS ni la table en cours de réinitialisation. La publication annulée ne peut pas remplacer le nouveau catalogue. |
| Retour du worker | Après la publication de fin, il ne reste aucun accès aux tables/handles. Le wrapper peut encore terminer sa pile ; les octets de cette pile restent alors visibles à l’admission suivante jusqu’au nettoyage FreeRTOS idle. Une éventuelle admission refusée garde le scan incrémental disponible. |
| Scanner Python | La recherche de tous les appels de production `beginAppScan/nextApp/endAppScan` confirme que seul le module catalogue les utilise. Aucun autre callback UI ne modifie concurremment cet état global dans les sources examinées. |

Une amélioration issue de cette revue a été intégrée par le propriétaire : une annulation avant la première entrée du worker est vérifiée avant `startScanIo()`, afin de ne pas commencer inutilement des E/S. Son test force cette séquence et vérifie zéro ouverture.

Aucune nouvelle course, utilisation après libération ou attente UI sur la fin du worker n’a été identifiée dans ces transitions. Cela n’est pas une preuve d’absence de toute course dans l’OS. Les limites restent l’annulation coopérative d’une E/S pilote déjà commencée, la validation de pile sur matériel et le partage du bus/mémoire entre cœurs.

Les scénarios exécutés, leurs frontières simulées et les empreintes sont dans `tools/verification/catalogue-0843.json` ; la compilation P4 est dans `catalogue-target-0843.json`. Aucun essai physique supplémentaire n’a été effectué par cette revue.
