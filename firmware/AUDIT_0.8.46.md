# Tab5OS 0.8.46 — réseau partagé, attentes et travail mémoire

La référence de travail est le projet **0.8.45 réellement livré**, retrouvé et vérifié par empreinte. Votre retour matériel est retenu : la SD dans Cartes semble maintenant stable ; les autres symptômes ne sont pas considérés comme résolus par les anciens tests. Cette livraison corrige des causes logicielles reproduites et réduit des coûts mesurables sur les sources. **Aucun essai sur une Tab5 physique n'a été effectué ici. Les flashs, resets radio/vidéo et claquements restent à valider.**

## Causes établies et corrections

| Module | Cause établie dans la référence | Modification vérifiée |
|---|---|---|
| Réseau/radio/Cartes | La radio conservait un mutex réseau pendant toute la lecture. Le Wi-Fi pouvait être connecté sans que Cartes puisse l'utiliser. | Exclusivité pendant le démarrage et ses allocations ; après démarrage, un flux radio établi peut coexister avec une opération réseau sérialisée. Les transitions du pilote attendent toujours la fermeture des transports. Budgets mémoire conservés. |
| Réseau | Une connexion Cartes pouvait être retenue pendant qu'un autre client attendait le mutex ; la condition n'était pas revérifiée après acquisition. | Nouvelle vérification sous verrou ; attente native FreeRTOS du retour des ressources, sans boucle `delay(1)`. |
| Cartes | Attente de file de tuiles de 250 ms avant une recherche nouvellement arrivée ; réveil périodique même sans travail ; pauses fixes de 60/220 ms après chaque tuile. | Notifications des deux files et des changements de cycle de vie. Sans connexion conservée ni travail, sommeil jusqu'à un événement. Suppression des pauses après tuile ; la file de résultat bornée et son consommateur assurent déjà la régulation. Les limites des fournisseurs et les reprises bornées sont conservées. |
| Cartes/UI | Le même message « Wi-Fi disponible requis » couvrait une absence réseau et une indisponibilité des ressources audio. Une lecture locale en pause était également exclue. | États distincts ; pause locale autorisée ; priorité du stockage conservée pour lecture locale active/enregistrement. Une fin d'ouverture radio relance les tuiles concernées. |
| Audio | Récepteur créé avant HTTP puis réveillé à chaque tick jusqu'à la fin de connexion ; sondage de l'anneau vide/plein ; décodeur local en pause réveillé chaque tick. | Notification de départ, attentes natives du stream buffer et de la file de commandes. Les joins avant destruction et la coopération du décodage actif restent en place. |
| Affichage | Zones du miroir synchronisées immédiatement, puis mêmes zones synchronisées à nouveau après rendu. | Synchronisation de l'union des zones une seule fois avant publication. Même barrière DMA, mêmes deux images, mêmes chemins de repli et de reprise ; aucune nouvelle allocation par trame. |
| Dessin | Tous les calques étaient recopiés et tout le masque de couverture remis à zéro au début d'un trait. | Historique limité aux pixels du calque effectivement modifié pour pinceau/gomme/déformation ; métadonnées et sélection conservées. Couverture initialisée par petits blocs touchés. Historique complet pour les opérations structurelles. |
| Média | Un clic sur l'onglet déjà actif reconstruisait son catalogue ; les allocations génériques des listes pouvaient se replier en RAM interne. | Pas de reconstruction sans changement d'onglet ; Actualiser reste disponible. Références/clés/noms de tri explicitement en PSRAM, avec nettoyage sur échec. |
| Réglages série | Callback bloquant jusqu'à 2 s, puis message d'échec alors que la demande pouvait réussir plus tard. Une restauration du même port après échec NVS pouvait aussi être annoncée comme réussite. | Demande, observation et annulation UI sans attente de mutex. Succès annoncé seulement après le résultat réel du worker. Les API synchrones du terminal/démarrage restent disponibles. |

L'arrêt par Pause de la radio **libérait déjà son transport dans les sources 0.8.45 testées**. Le signalement sur appareil « Wi-Fi indisponible après pause radio » n'est donc pas présenté comme entièrement expliqué par le verrou de lecture. Les tests actuels couvrent ce chemin, mais une reproduction matérielle reste nécessaire si le symptôme persiste.

## Structure, mémoire et cœurs

La répartition existante est réutilisée : UI sur cœur 1, décodage audio/vidéo sur cœur 0, récepteur radio et Cartes sur cœur 1. Le second cœur du P4 était déjà actif. Les changements rendent plusieurs workers dormants sur événements et diminuent le travail mémoire avant rendu ; ils ne déplacent pas arbitrairement toutes les tâches sur le cœur audio. Le C6 exécute son propre firmware de communication et ne partage pas le heap du P4.

Les données volumineuses compatibles sont déjà en PSRAM. Les piles ordinaires, certains objets système et buffers DMA demandent encore des régions internes et des blocs contigus. **32 Mio de PSRAM libres ne remplacent pas un bloc interne manquant.** Les 768 Kio physiques HP ne constituent pas un heap de 768 Kio : la configuration utilise notamment 256 Kio de cache L2, puis code, données et réserves. TCM/SPM, LP SRAM et capacités DMA ne sont pas additionnées deux fois aux chiffres internes.

Aucune marge d'admission ni pile n'a été diminuée pour faire artificiellement passer un lancement. Les données non enregistrées d'une application cachée restent volontairement conservées. Le bilan séparé distingue allocations instrumentées, sections du firmware et mesures physiques absentes.

## Inventaire des délais

L'audit porte sur **1 377 fichiers** du firmware et de ses ports compilés. La référence contient 43 appels lexicaux `delay`, 93 `vTaskDelay` et 74 `xSemaphoreTake`. Le JSON/CSV joint donne les appels, arguments, lignes, empreintes et contextes ; il inclut les variantes du préprocesseur, pas seulement les appels exécutés.

Tous les délais ne sont pas des erreurs. Une attente de file se termine dès l'événement ; un tick dans le décodage actif permet aux tâches idle et au watchdog de fonctionner. Un `delay(1)` dans un callback UI, en revanche, ne rend pas la main à LVGL. Les accès SD/réseau/I²C et appels de codecs peuvent bloquer sans contenir le mot `delay` : ils font partie de la revue.

Le détail et les dettes restantes figurent dans `tools/audit-0846/waits-memory.md`, `waits-memory-inventory-current.json` et `.csv`, et `tasks-memory-0845.json`. L'inventaire n'est pas présenté comme une preuve de suppression de tout blocage indirect.

## Validation et limites

Les suites exécutent les implémentations de production, avec le SDK/LVGL réel lorsque disponible et des frontières matérielles explicitement adaptées. Comparaison au ZIP livré, tests de propriété, erreurs d'allocation, concurrence, annulation et cycles de fermeture sont conservés avec leurs scripts et empreintes. La compilation complète, les sections, les avertissements et les partitions sont consignés dans `BILAN_RESSOURCES_0.8.46.md` et `VERIFICATION.md`.

La relecture indépendante a notamment conduit à corriger le résultat NVS trompeur avant le gel final. Elle a aussi identifié deux lectures du volume depuis le worker audio sans le verrou utilisé par le réglage UI ; leur réapplication à la sortie est désormais protégée par ce même verrou court. Cela corrige une course de données, sans attribuer le claquement physique à cette seule cause. Une suspicion sur l'annulation de très grands documents Dessin a été retirée après vérification des gardes de l'API publique ; aucun correctif spéculatif n'a été ajouté. Un test différentiel indépendant compare 5 000 opérations mixtes après chaque étape : pixels, sélection, métadonnées, RGB565 et annuler/rétablir identiques à la référence.

Partitions, chiffrement, coffre, données personnelles et choix d'autosauvegarde sont préservés. Aucune carte utilisateur n'est montée ni modifiée par ces travaux. L'OTA garde son organisation. Les applications ELF restent natives sans isolation matérielle ; les exclusions de noms supprimées précédemment ne sont pas réintroduites.

## Problèmes résiduels classés

| Priorité | Symptôme ou dette | Investigation/action nécessaire |
|---|---|---|
| Haute | Flashs Dessin/Média et au repos | Comparer `d` avant/après : activation du présentateur, publications, invalidations et underruns DSI. Le balayage DMA continue au repos. La réduction de travail cache n'établit pas la disparition physique du flash. |
| Haute | Reset ou échec intermittent radio/vidéo stream | Journal complet de démarrage et de panic/reset, dernière opération réseau, blocs libres et piles. Les défauts corrigés et tests hôte ne suffisent pas à identifier tout reset restant. |
| Haute | Radio refusée après Cartes, mémoire BLE/Mesh | Comparer total interne, plus grand bloc, minimum et retour des workers après fermeture. Les cartes de ressources distinguent rétention volontaire et fuite. Aucun nouvel essai BLE sur appareil. |
| Haute | Wi-Fi signalé indisponible après pause radio | Reproduire en distinguant lecture radio, pause radio, pause MP3, import et simple affichage. Garder le texte exact et les logs ; pause radio était déjà libératrice dans la référence. |
| Moyenne | Claquement au démarrage MP3 | Mesurer sortie/codec et PCM initial sur matériel. Aucun nouveau reset, fade ou délai arbitraire n'a été ajouté. |
| Moyenne | Admission mémoire UI jusqu'à 60 ms, décodage image/verrou jusqu'à 3 s, sauvegardes/transforms synchrones, tri SD taille/date | Reporter l'opération entière avec propriété explicite des résultats et choix utilisateur conservés ; ces chemins restent recensés, sans fausse promesse de non-blocage général. |
| Moyenne | Worker audio de maintenance 5 s, polling série 5 ms, appels répétés WiFi.setSleep | Dette d'activité au repos. La maintenance gère aussi l'économie d'énergie ; la supprimer sans centralisation cohérente régresserait les réglages. |
| Haute si reproduit | Arrêts NimBLE, USB/DMA, récupération SD Python pouvant attendre sans borne | Ne pas libérer de ressources encore détenues. Journaux des retours pilotes et callbacks nécessaires ; pas de destruction forcée pour masquer le blocage. |
| À surveiller | Perte SD sous zoom intense | Retour matériel positif sur 0.8.45 retenu. Correctifs FS/reconnexion préservés ; zoom libre pendant téléchargement et recherche Internet puis locale conservés. Test de non-régression proposé. |
| À confirmer | Anciens cas USB imprimante/remontage après formatage, affichage Mesh, applications externes | Non validés matériellement par cette livraison ; fonctions et données conservées. Ne pas les considérer réparés par une compilation. |

Cette version est une amélioration vérifiée des sources, pas une certification de résolution de tous les symptômes sur appareil. La procédure courte de reproduction et les relevés attendus sont dans `TESTS_TAB5_0.8.46.md`.
