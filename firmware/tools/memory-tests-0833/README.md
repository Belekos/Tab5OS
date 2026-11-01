# Vérification du gestionnaire mémoire 0.8.33

Depuis la racine de la livraison, avec GCC C++17 et pthreads :

```sh
python3 tools/memory-tests-0833/run.py
python3 tools/memory-tests-0833/run_regression.py
python3 tools/memory-tests-0833/run_integration.py
```

Les trois scripts compilent `Tab5ResourceManager.cpp` réel sous AddressSanitizer et UndefinedBehaviorSanitizer. Les mesures de tas et les ressources libérables restent contrôlées par le banc hôte.

- `run.py` couvre les domaines de mémoire, la fragmentation, les réservations concurrentes, les demandes depuis les workers, l’attente bornée d’un retrait différé de pile, l’absence de purge différée après refus et la protection du propriétaire visible/demandeur. Il exécute 400 cycles réserve/allocation/libération et 400 changements de propriétaire sans expiration de cache. Les marges restent de 32 Kio internes et 512 Kio PSRAM.
- `run_regression.py` exécute cinq scénarios identiques contre les sources livrées en 0.8.32, conservées sans modification dans `baseline-0832`, puis contre la 0.8.33. Il reproduit les purges à 30 s, la maintenance sans lancement, l’éviction du premier plan et du demandeur, et la purge différée d’un domaine sans rapport avec un échec d’allocation. Le service Arduino et le callback d’échec sont extraits verbatim ; leurs frontières matérielles sont simulées. Les cinq purges doivent avoir lieu dans la référence et être absentes dans la correction, tandis que le diagnostic d’allocation reste présent.
- `run_integration.py` compile le routage et les callbacks réels extraits du sketch. Il vérifie 11 configurations de propriétaire, dont Atelier Image, et 9 configurations de cache : Cartes visible ou utilisée par impression/JPEG, explorateur et médias visibles sous un panneau d’outils, nettoyage de listes masquées avec invalidation des pointeurs et arrêt des parcours avant destruction.

Les rapports `tools/verification/memory-0833.json`, `memory-regression-0833.json` et `memory-integration-0833.json` identifient les sources et scripts utilisés. Le premier conserve une empreinte du bloc d’enregistrement ; le dernier lie aussi le sketch entier à son résultat. Les registres .31/.32 restent historiques.

Limites : aucune exécution FreeRTOS ni allocation matérielle sur Tab5, aucun test Wi-Fi ou Git réseau dans ces suites. Elles ne prouvent pas l’origine physique de tous les flashs signalés. Le rendu, le cycle de vie audio et l’application image sont vérifiés séparément. LeakSanitizer est désactivé dans cet environnement hôte ; AddressSanitizer et UndefinedBehaviorSanitizer restent actifs.
