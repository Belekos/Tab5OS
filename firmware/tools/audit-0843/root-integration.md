# Intégration UI, horloge et ouverture des applications — 0.8.43

## Méthode

Comparaison avant/après sur les vraies fonctions extraites des main 0.8.42 et 0.8.43 : mises à jour horloge/indicateur audio, politique RTC/NTP, callbacks de réglage, dispatch ouvrir/exécuter et lambda d'exécution ELF. Les accesseurs snapshot/indicator/WiFi d'Audio et la structure de snapshot sont ceux de production ; le classificateur des formats d'image est lié directement. Les seules frontières simulées sont String/mutex RTOS, horloge, RTC/SNTP et appels aux applications/fichiers. LVGL 9.6 est réel, ASan/UBSan actifs. Pas de mesure de RAM physique, de temps CPU ESP32 ni d'exécution d'instructions ELF sur hôte.

Scripts : `tools/root-integration-tests-0843/run.py`, `main.cpp`. Rapport : `tools/verification/root-integration-0843.json`, journaux adjacents. `verify.py` refuse la réutilisation si un extrait effectivement exécuté a changé ; les changements sans rapport dans le grand main peuvent être réconciliés avec leurs deux empreintes explicitement conservées.

## Résultats contrôlés

| Scénario isolé, 60 s simulées | Copies String 0.8.42 | Copies String 0.8.43 |
|---|---:|---:|
| Indicateur audio au repos, appels UI toutes les 8 ms | 1 410 | 0 |
| Horloge statique et état WiFi inchangé, relevés chaque seconde | 360 | 0 |
| Service RTC/NTP désactivé, vérifications chaque 250 ms | 1 440 | 0 |

Ce sont des compteurs de copies du type hôte, pas des nombres d'allocations ou des mesures du tas de la tablette. La différence de cadence 250/256 ms issue de la boucle à 8 ms est incluse dans la mesure de l'ancien indicateur ; aucun arrondi à 1 440 n'est présenté pour ce scénario.

Les séquences au repos n'émettent aucune invalidation LVGL supplémentaire. Après un refus temporaire du mutex, le prochain relevé ne transforme pas le WiFi en état faux. Une demande forcée de l'indicateur reste en attente si le mutex est occupé ; elle est rejouée même si la révision n'a pas changé. Cela maintient la bonne visibilité lors du passage Accueil ↔ Éditeur. Les retours chariot/sauts de ligne du titre sont remplacés comme auparavant.

## RTC/NTP et politique utilisateur

Les deux versions conservent les mêmes transitions : activation hors ligne en attente ; connexion lançant une seule synchronisation ; absence de relance tant que la connexion ne change pas ; réception d'une date invalide sans écriture RTC ; réception valide écrivant le RTC puis arrêtant SNTP ; perte de connexion avec pile encore active ; désactivation explicite ; changement de fuseau uniquement quand NTP est activé ; initialisation de compilation seulement si le RTC ne fournit pas de date valide. Les callbacks de réglage sont envoyés depuis de vrais objets LVGL. Les deux scénarios finissent avec cinq demandes NTP identiques.

Le test ne simule pas le réseau ni la précision d'une horloge matérielle. Il vérifie que remplacer les snapshots textuels par des états scalaires ne change pas la politique existante.

## Suppression des redirections historiques

En 0.8.42, ouvrir, exécuter et passer par le hook de lancement pour `/Applications/AtelierImage.app.elf` redirigeaient tous vers Dessin. En 0.8.43, les trois chemins transmettent le fichier ELF au lanceur, avec argument et langue conservés. Un projet `.t5i` continue à ouvrir Dessin, et le bouton Exécuter ne le prend pas pour un programme.

Les ELF Quake, Duke3D et un ELF personnalisé à extension majuscule suivent bien les chemins d'ouverture/exécution normaux. Un dossier portant un nom `.elf` reste un dossier. Les chemins shell, firmware et Python conservent leurs associations. La découverte dans le catalogue est vérifiée séparément dans son propre lot.

**Aucun test sur Tab5 physique n'a été effectué.** Ces essais valident des décisions et événements du code réel, pas l'absence de flash ni la fluidité réelle de l'écran.
