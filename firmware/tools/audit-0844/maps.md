# Cartes — corrections vérifiées 0.8.44

Base : sources réelles livrées en 0.8.43. Fichier de production modifié : `Tab5Map.cpp`. Les API publiques, préférences, cartes hors ligne, marqueurs Meshtastic, formats et capacités des caches restent conservés.

## Causes établies dans les sources

1. **Retraite de la tâche retardée après annulation.** Après chaque tuile, `taskLoop()` attendait systématiquement 60 ms, ou 220 ms lorsque la radio était prioritaire, même après fermeture de Cartes ou changement de génération. Sa pile de **16 384 octets de mémoire interne** restait allouée pendant cette attente. Ce délai peut croiser les 60 ms d'admission récupérante de la radio. C'est une pression transitoire établie, pas une fuite permanente démontrée ni la preuve que tous les refus radio viennent de cette tâche.
2. **Lecture HTTP non coopérative pendant les en-têtes.** Les vérifications d'annulation du moteur entouraient `GET()` et surveillaient les écritures de corps, mais le transport HTTP pouvait attendre des en-têtes sans consulter l'annulation de Cartes ou la priorité média. Le worker de priorité 2 sur le même cœur que l'interface pouvait aussi empêcher l'inactivité de ce cœur pendant une attente active de `Stream::timedRead()`.
3. **Succès d'écriture annoncé sans vérifier la synchronisation ni la fermeture.** Le cache de tuiles, les lieux et la demande de téléchargement ne vérifiaient que les comptes d'octets. `flush()`/`close()` pouvaient échouer alors que l'ancien fichier était remplacé. Une erreur réelle `ENOSPC` pouvait être effacée par la fermeture, puis annoncée comme `EIO`, ce qui demandait à tort une récupération du support.
4. **Sonde de présence ambiguë avant publication.** `exists()==false` était traité comme une absence, sans distinguer `ENOENT` d'une erreur de lecture des métadonnées. Les sauvegardes pouvaient être retirées avant la vérification de l'ancien fichier.
5. **Bornes d'E/S incomplètes.** Les boucles coopératives n'écartaient pas un retour supérieur à la demande, notamment `size_t(-1)` après invalidation d'un handle. Les leases limitent ce risque ; aucune atteignabilité matérielle de cet état pendant les incidents signalés n'est affirmée.

## Corrections

- Le délai entre tuiles ne s'applique qu'à une génération encore active sans demande de priorité réseau média. Les délais utiles de 60/220 ms sont conservés pendant le travail normal. La tâche quitte immédiatement sa boucle lorsqu'elle est désactivée, puis son stockage est récupéré par FreeRTOS. Un relevé de marge de pile est émis à sa fermeture ; aucune pile n'a été réduite.
- Les clients HTTP/TLS de Cartes réutilisent `Tab5CooperativeClient`. Une liaison d'annulation limitée à la requête est installée et retirée sur **la tâche propriétaire** ; aucun pointeur vers le contexte de pile n'est conservé après son retour. En attente d'en-têtes/corps, le transport cède réellement un tick et peut fermer sa connexion sur annulation. Le DNS, la connexion TCP et la poignée TLS restent soumis aux bornes des pilotes ; ils ne sont pas présentés comme immédiatement préemptibles.
- Les écritures vérifient octets, indicateur d'erreur, taille, synchronisation et fermeture avant publication. La **première erreur réelle est conservée** jusqu'au signalement, y compris après nettoyage du fichier temporaire. Les JSON restent sérialisés directement dans le fichier, sans seconde copie complète en mémoire.
- Les sondes et remplacements atomiques existants sont réutilisés. L'ancien primaire reste en place tant que l'écriture temporaire n'est pas entièrement validée ; une promotion ratée tente sa restauration et conserve le backup si la restauration échoue. Le couple `.part`/`.bak` existant reste employé. Le fichier de demande de région bénéficie aussi du remplacement avec restauration.
- Un répertoire de cache déjà existant est accepté comme tel. Un fichier ordinaire à cet emplacement est refusé ; aucune récupération du bus ne peut réparer cette collision de chemin.
- Les valeurs impossibles de lecture/écriture sont rejetées avant toute addition ou avancée de pointeur.

## Tests exécutés

`tools/maps-tests-0844/run.py` extrait les méthodes **exactes** de production, avec adaptateurs hôtes pour le système de fichiers, les sockets et FreeRTOS. AddressSanitizer et UBSan sont actifs. LeakSanitizer n'est pas utilisable sous le traçage de cet environnement ; les handles et les payloads à capacités sont suivis explicitement.

| Vérification | 0.8.43 | 0.8.44 |
| --- | --- | --- |
| Délai prédéfini après annulation d'une tuile | 60 ms | 0 ms |
| Même annulation, priorité radio | 220 ms | 0 ms |
| Génération périmée / priorité réseau média | 60 / 60 ms | 0 / 0 ms |
| Délai utile d'une génération active | 60 / 220 ms | 60 / 220 ms |
| Échecs injectés de sync/close publiés à tort | 2/2 | aucun : ancien primaire préservé |
| Écriture courte `ENOSPC` | signalée `EIO` | signalée `ENOSPC` |

Les millisecondes ci-dessus comptent les appels explicites à `vTaskDelay` dans les méthodes réelles ; ce ne sont pas des mesures de temps de réponse sur une Tab5. Les tests couvrent 100 écritures répétées, carte pleine, manque de mémoire, erreur de sync, erreur de close, échec de stat, de promotion et de rollback, collision de répertoire et retour d'E/S impossible. **1 123 assertions**, avant/après inclus, réussies ; ressources suivies rendues.

Le corpus de régression de recherche hors ligne a été exécuté sur les sources 0.8.44 : **540 852 assertions** réussies, dont 15 sessions / 75 recherches, marqueurs sauvegardés/Mesh, absence de Wi-Fi, perte de Wi-Fi pendant HTTP, annulation pendant lecture, OOM et index réel. Ces nombres sont des assertions, pas autant de scénarios indépendants ni de cycles complets de l'OS.

Compilation de `Tab5Map.cpp` avec la base de commandes **0.8.44** et le compilateur P4 GCC 14.2.0 / SDK M5Stack 3.3.9 : réussie. Cadres de pile statiques : `taskLoop` 1 040 octets, `downloadTile` 320, `downloadSearch` 304. Ils ne représentent pas la consommation cumulée avec HTTP/TLS ni une mesure de marge sur appareil. Le bilan complet de compilation, Flash et mémoire est fourni par le build de la version.

Preuves : `tools/verification/maps-0844.json`, `maps-regression-0844.json`, `maps-target-0844.json`, et journaux associés.

## Limites et investigations sur appareil

- L'indisponibilité physique de la SD sous charge peut aussi venir du bus, de l'alimentation, du pilote, de la carte ou d'un autre accès concurrent. Les corrections de publication et de faux `EIO` ne prouvent pas qu'elle a disparu. Aucun formatage, remount supplémentaire ni reset périphérique n'a été ajouté ici.
- L'intégrité FAT lors d'une coupure électrique pendant renommage n'est pas garantie par un renommage logiciel. Un échec simultané de promotion et de restauration laisse le backup conservé et doit être diagnostiqué.
- La tâche réseau Cartes conserve sa pile tant qu'elle travaille ou attend pendant que Cartes est ouverte ; cette version n'introduit pas une nouvelle architecture de tâches, ni une réduction arbitraire de sa pile. Les tables, les files de données et les images sont déjà en PSRAM ; les blocs internes contigus et DMA requis par radio/Bluetooth ne sont pas remplaçables par cette PSRAM.
- Les messages de connectivité de l'interface sont intégrés séparément dans le sketch principal. Une contention normale de mutex n'a **pas** été démontrée comme cause de faux Wi-Fi : `snapshot`/`routingSnapshot` utilisent un verrou bloquant. Il faut distinguer option Wi-Fi activée, association effective et résultat HTTP/TLS.
- Aucun essai sur Tab5 physique. À relever : mémoire interne libre et plus grand bloc avant Cartes, pendant téléchargement, immédiatement après fermeture, puis après lancement radio ; lignes `[STACK] tab5_map_net`, `[MAP][NET]`, `[SD][ERREUR]`, raison d'admission radio et raison de reset. Ne pas inclure de secrets dans ces journaux.
