# Comparaison indépendante des avertissements

Les sources de référence 0.8.47 ont été recompilées avec les options de diagnostic
utilisées pour 0.8.48 (`-Wall -Wextra`, suppression de `-w`, erreurs sur retours
manquants, valeurs non initialisées et usages après libération), sur la même
chaîne ESP32-P4 restaurée.

| Unité | 0.8.47 | 0.8.48 | Nouvel avertissement |
|---|---:|---:|---:|
| Audio | 1 | 1 | 0 |
| Vidéo | 1 | 1 | 0 |
| DisplayPresent | 1 | 1 | 0 |
| DisplayCache | 1 | 1 | 0 |
| DisplaySignals | 0 | 0 | 0 |
| ImageEditor | 50 | 50 | 0 |
| Programme principal, comparaison directe C++ | 357 | 357 | 0 |

Chaque compilation a terminé sans erreur; les multiensembles des messages sont
identiques avant/après. Ce contrôle ne prétend pas compiler une deuxième image
complète 0.8.47 : les dépendances sont celles, épinglées, de la compilation 0.8.48.

Pour le programme principal, la comparaison directe ajoute dans les deux fichiers
temporaires le prototype `selectedOpenEvent` normalement généré par Arduino et
préinclut `Arduino.h`. La compilation habituelle du main généré Arduino affiche
316 avertissements : ses directives de génération changent la couverture des
messages d'indentation. La différence 357/316 n'est **pas** annoncée comme un
nettoyage ni une amélioration des sources.

Les avertissements conservés concernent principalement les API LVGL dépréciées,
l'initialisation partielle d'agrégats, l'indentation historique et les fonctions
inutilisées. Les unités Audio/Vidéo/Present/Cache partagent un avertissement
`-Wliteral-suffix` situé dans `esp_private/periph_ctrl.h` du SDK. Les avertissements
ne sont pas supprimés ni cachés par de nouveaux pragmas. L'égalité de ces messages
ne remplace pas les tests de fonctionnement et ne qualifie pas le système pour
la production.

Preuves et commandes : `tools/verification/independent-warning-comparison-0848.json`;
journaux dans `tools/verification/warning-comparison-0848`.
