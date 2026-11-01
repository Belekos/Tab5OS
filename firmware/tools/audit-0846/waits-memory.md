# Audit transversal des attentes, tâches et mémoire — 0.8.46

L’état initial est le ZIP 0.8.45 livré, SHA-256 `46fd5da89ebb693a61d3ab34e2d3f3480a714aff76c095ce339f290ee8641573`. Le retour matériel du propriétaire indique que la SD dans Cartes tient désormais ; les flashs, démarrages radio/vidéo et claquements restent des symptômes ouverts. Le relevé ci-dessous distingue un mécanisme de code établi d’une cause matérielle seulement possible. Aucun temps CPU, débit PSRAM, minimum de pile ou délai physique n’a été mesuré ici.

## Inventaire reproductible et limites

`inventory_waits.py` lit tous les fichiers C/C++/en-têtes/includes à la racine du sketch, ses ports et dépendances `src/`, ses en-têtes `py/`, et le noyau Dessin réellement inclus depuis `tools/native-apps/image-editor`. Les commentaires et chaînes sont masqués avant recherche. Les anciens tests et les programmes ELF indépendants ne sont pas mélangés au firmware. Le JSON donne une empreinte de chaque source, la ligne, l’API, ses arguments, le contexte et sa classe. Le CSV fournit le même détail pour filtrage. Le scan conserve les variantes du préprocesseur : une occurrence n’est pas une preuve d’exécution, et un appel de création dans un template n’est pas une tâche supplémentaire.

La référence comprend 1 377 fichiers, 43 appels lexicaux `delay`, 93 `vTaskDelay`, 74 `xSemaphoreTake`, une prise récursive, et deux appels du helper `pauseAdmission`. Les boucles candidates sont aussi listées, y compris les boucles de parseurs qui ne sont pas des attentes. Les compteurs courants se recalculent après le gel des sources. L’inventaire **ne prétend pas détecter automatiquement tous les blocages indirects** : appels réseau, SD, I²C, codec, callbacks et méthodes virtuelles ont été examinés séparément ci-dessous. Les scripts Python chargés depuis SD et les ELF tiers n’ont pas de borne de coopération garantie.

Commande depuis la racine du projet :

```sh
python3 tools/audit-0846/inventory_waits.py --sketch Tab5OS_Arduino_v0.8.46_Ecriture --baseline-zip /chemin/Tab5OS_0.8.45_SD_Radio.zip --output tools/audit-0846
```

## Ce que signifie un délai sur cette configuration

Le SDK verrouillé M5Stack Arduino 3.3.9 appelle `vTaskDelay(ms / portTICK_PERIOD_MS)` dans `delay()`. Son `CONFIG_FREERTOS_HZ` vaut 1000 : un tick vaut 1 ms. `delay(0)`/`yield()` ne garantissent pas que la tâche idle de priorité inférieure sera exécutée. Un tick de sommeil est donc utile dans un décodeur ou une boucle continuellement alimentée : le supprimer peut affamer idle et provoquer le watchdog.

Un `delay(1)` placé **dans un callback UI** cède le processeur aux autres tâches, mais ne rend pas la main à la boucle LVGL. Pendant la totalité de ce callback, les entrées et les autres services UI attendent. Inversement, `xQueueReceive(..., timeout)` et `xEventGroupWaitBits(...)` se réveillent à l’arrivée d’un événement : leur timeout maximal ne constitue pas un délai imposé à chaque événement. `xSemaphoreTake(..., 0)` ne dort pas. Il serait incorrect de remplacer ces trois catégories indistinctement.

## Blocages établis et état de correction

| Chemin | Fait établi dans la 0.8.45 | Impact et décision |
|---|---|---|
| Réglages série → `Tab5Serial::configure` | Le clic UI attend jusqu’à 400 × 5 ms. La prise du mutex peut aussi attendre l’installation du pilote/NVS. | Correction 0.8.46 par le responsable de l’intégration : demande et observation non bloquantes, résultat annoncé seulement après achèvement. Les API synchrones du démarrage/terminal restent disponibles. Voir tests série de la livraison. |
| Radio → porte réseau → Cartes | La radio retenait l’accès réseau exclusif pendant sa lecture établie et l’UI de cartes prolongeait cette priorité. Le chemin réel de pause ferme la lease : le symptôme radio en pause n’est pas reproduit par cette seule cause. | Cause établie pour radio active. L’import confondait aussi refus de ressources et absence Wi-Fi ; la lecture locale en pause restait exclue. Révision des admissions démarrage/flux établi et de l’UI en 0.8.46 ; le cas radio en pause reste à confirmer si reproduit sur appareil. |
| `Manager::reserveRecovering(..., collectorThread=true)` | Après collecte, jusqu’à 60 ms de réessais avec un tick d’attente dans l’appelant UI. Le verrou d’admission lui-même peut attendre de façon bornée. | Dette résiduelle : latence de lancement certaine sous manque de ressources ; durée réelle non mesurée. Ne pas supprimer les budgets ni forcer le lancement. Une évolution doit reporter l’opération entière et son état utilisateur jusqu’à la fin du retrait des tâches. |
| Dessin, premier appui de pinceau | `ie_brush_begin` alloue et remet à zéro un masque de largeur × hauteur, puis `ie_gesture_begin` duplique tous les calques et le masque de sélection pour l’annulation. Chaque allocation passe l’admission PSRAM depuis l’UI. | Correction 0.8.46 : copie du seul calque modifié pour pinceau/gomme/déformation, métadonnées/sélection conservées, couverture initialisée par blocs touchés. Les opérations structurelles gardent leurs instantanés complets. Tests de propriété, annuler/rétablir et défauts exécutés dans `drawing.md`. Le calque actif reste copié ; **les flashs physiques ne sont pas déclarés résolus**. |
| Dessin : charger, sauvegarder, transformer, régler les couleurs, redimensionner | Plusieurs opérations font leur boucle complète dans le callback ; `cooperate()` ne fait qu’un `delay(1)` tous les 16 ms. Sélection/remplissage disposent déjà de jobs découpés. | Dette UI réelle ; étendre les jobs existants aux autres opérations est préférable à une réécriture. Ne pas déplacer des appels LVGL sur une tâche de fond. |
| Visualisateur/illustration EPUB | Deux `Tab5ImageLock(3000)` dans le main, suivis d’un décodage synchrone. | Jusqu’à 3 s d’attente du verrou, puis travail du codec, depuis l’UI. Risque de gel en coexistence ; conversion en job avec propriété du résultat à concevoir, sans réutilisation d’un canvas affiché pendant écriture. |
| Écriture : `beforeSwitch`, `save(..., synchronous=true)` | `SaveJob` existe mais certains chemins consomment tous ses pas dans une boucle `delay(1)` avant de revenir. | Bloque UI pendant une transaction déjà demandée. Reporter l’action suivante jusqu’à fin de SaveJob ; conserver exactement les choix de sauvegarde existants. |
| Écriture : liens de références, `resume` | Indexation de plusieurs documents et chargement de session terminés dans l’appel UI ; le chargement de session est limité à 16 Kio mais peut attendre le support. | Dette résiduelle de navigation, indépendante du bug radio. Les limites d’octets ne constituent pas une borne de temps d’un périphérique défaillant. |
| Ancienne zone de texte : `atomicWriteText` du main | Boucle d’écriture avec `delay(0)`, commit/flush synchrones. | À maintenir dans l’inventaire même si ce n’est pas le chemin principal Écriture. Pas de preuve qu’elle explique les flashs au repos. |

Les corrections audio/affichage de cette livraison ont leurs tests et diagnostics spécifiques. Ce rapport ne transforme pas les dettes ci-dessus en corrections réalisées.

## Attentes conservées avec raison et risques résiduels

- La boucle UI finit par `delay(min(lvWait, 8 ms))`, minimum 1 ms, ou maximum 30 ms écran éteint. Touch/M5 et les files HID sont encore sondés depuis cette boucle. Ce sommeil borne la cadence sans redessiner l’écran à chaque tour. Le curseur est déjà en mode LVGL événementiel, les mouvements de survol sont regroupés, les boutons/drag/roulette restent ordonnés. Passer tout le système à un sommeil infini exigerait que **toutes** ces sources réveillent la tâche ; ce n’est pas assuré par les pilotes actuels.
- Le service série permanent sonde ses files toutes les 5 ms. C’est une dette d’activité au repos, non une preuve de flash. Une évolution pourrait utiliser les événements UART et les notifications USB ; il faut préserver le mode debug, le routage stdout, l’écriture bornée et les changements de port.
- Les tâches USB hôte/HID/MSC attendent des files/événements. Les délais 100 ms de leur contrôle de cycle de vie ne retardent pas nécessairement les rapports HID, qui arrivent par callback. Les transferts de souris ne passent pas par une attente active de 100 ms avant chaque mouvement.
- Les délais des workers HTTP/SSH/recherche servent aux timeouts, à l’annulation ou à la coopération des boucles continuellement alimentées. La limitation du géocodeur Internet protège aussi le service distant ; ne pas l’enlever pour annoncer un gain local.
- L’arrêt radio attend la fin réelle du récepteur avant de libérer HTTP/TLS/anneau. Le passage à une attente illimitée après le timeout diagnostique protège d’un use-after-free. Le bon correctif est de rendre l’opération réseau annulable, pas de libérer le transport sous son utilisateur.
- L’imprimante USB et le port SDR attendent parfois sans borne le callback après halt/flush. La propriété DMA interdit de libérer le buffer avant restitution. Si le contrôleur ne rend pas le transfert, le worker peut rester bloqué et retenir mémoire/verrou ; aucun test avec imprimante physique n’a été fait. Il faut enregistrer code de halt/flush, callback et génération puis traiter une panne réelle de l’hôte avec son API, sans destruction forcée.
- Meshtastic attend `hostExited` après `nimble_port_stop()` même si celui-ci retourne une erreur. C’est un risque établi de rétention sans borne si le host ne sort pas ; cela ne bloque pas directement LVGL. Ne pas appeler `nimble_port_deinit()` ni libérer le modèle pendant un host actif. Un arrêt explicite à états avec observation de la réponse du host serait nécessaire ; aucune erreur physique de ce chemin n’a été reproduite.
- Python op98 attend la récupération SD au plus 12 s, puis réacquiert sa lease sans borne avant de reprendre la VM, annulation incluse. C’est une garantie de propriété, mais une récupération qui ne rendrait jamais son exclusivité laisserait le script en attente. La borne 12 s ne borne donc pas toute l’opération.
- ELF : la coopération et l’annulation sont volontaires. Une tâche native qui ne les respecte pas peut monopoliser un cœur ou écrire dans la mémoire de l’OS. Le chargeur n’offre pas d’isolation matérielle. Sa suspension finale avec pile PSRAM est volontaire : l’UI attend l’état suspendu avant `vTaskDeleteWithCaps`.

## Organisation réelle des deux cœurs et des piles

Le tableau JSON `tasks-memory-0845.json` contient les 33 sites de création de tâches applicatives/ports, plus UI et Speaker, leurs arguments et la configuration SDK utilisée. Ce ne sont **pas 35 tâches simultanément actives**. Les wrappers dans `Tab5Task.h` ne sont pas comptés comme des tâches supplémentaires.

| Activité | Cœur P4 | Priorité | Pile déclarée |
|---|---:|---:|---:|
| UI Arduino/LVGL | 1 | 1 | 24 Kio |
| Cartes réseau/recherche | 1 | 2 | 16 Kio |
| Réception radio | 1 | 4 | 16 Kio |
| Décodage audio | 0 | 5 | 16 Kio |
| Speaker M5Unified | 0 | 6 | 3 328 octets avec DMA 512 |
| Vidéo | 0 | 5 | 24 Kio |
| Hôte NimBLE / service Mesh | 0 | 5 / 2 | 5 / 8 Kio |
| MicroPython | 0 | 1 | 32 Kio |
| ELF standard / ELF grande pile | 0 | 2 | 16 Kio interne / taille déclarée PSRAM |
| Git | 0 | 1 | 40 Kio |
| Impression / calcul de pages | 0 | 1 | 24 Kio |
| Terminal | 0 | 1 | 16 Kio |
| Série | 0 | 2 | 4 Kio |
| USB host / gestion HID | sans affinité explicite | 3 | 4 / 6 Kio |
| SDK HID / MSC / imprimante | 0 | 4 / 3 / 3 | 4 / 4 / 4 Kio |

Le cœur 0 est donc déjà utilisé, notamment pour audio, vidéo, USB et interprètes. Tout déplacer sur ce cœur affamerait potentiellement l’audio. Sur le cœur 1, les workers réseau prioritaires peuvent préempter l’UI : leurs boucles actives doivent bloquer sur la disponibilité ou coopérer en vrai tick. Ni une priorité plus basse généralisée, ni des pauses arbitraires ne corrigent la propriété des buffers ou les blocages UI.

Sur un démarrage réussi, les seules piles UI+série+USB listées ci-dessus totalisent **50 Kio déclarés**, avant TCB, files, hôtes ESP-IDF, TCP/IP, timer, transport ESP-Hosted, buffers et métadonnées. Ce chiffre est calculé depuis les arguments, pas mesuré sur carte. Le C6 possède son firmware et sa mémoire, reliés par ESP-Hosted : ce n’est pas un troisième worker FreeRTOS partageant directement les buffers du P4. L’utiliser comme processeur général exigerait un protocole/firmware supplémentaire, hors correctif ciblé.

## SRAM, TCM/SPM, LP RAM, PSRAM et admissions

- Le SDK P4 sélectionné configure 768 Kio physiques HP et **256 Kio de cache L2** ; l’ensemble physique n’est pas un heap disponible à `malloc`. Le code lié, les données statiques, les réserves système et les contraintes de capacités consomment aussi de la mémoire. On ne doit donc pas remplacer « heap interne exposé » par « 768 Kio libres ».
- TCM/SPM désigne la même banque 8 Kio sur cette cible, LP SRAM 32 Kio est distincte. Les diagnostics échantillonnent les régions effectivement enregistrées (`INTERNAL|8BIT`, `INTERNAL|DMA|8BIT`, `TCM|8BIT`, `RTCRAM|8BIT`). Les valeurs de régions et de capabilities se recoupent : DMA n’est pas un volume supplémentaire à additionner à l’interne. Les minimums libres agrégés sont une somme des minima propres aux régions, pas nécessairement le minimum simultané observé.
- La PSRAM 32 Mio configurée à 200 MHz reçoit déjà documents, images, historique Dessin, canvas/caches Cartes, objets LVGL, dictionnaires de langue, modèle/historique Mesh, anneau radio et TLS via allocateur compatible. Les framebuffers DMA d’affichage ont leurs propres règles d’alignement/cache et ne doivent pas être déplacés vers un malloc quelconque.
- Les piles de tâches ordinaires, TCB/files FreeRTOS, objets système, certains buffers de transports et I2S DMA restent internes selon leurs APIs. Une grande quantité de PSRAM libre n’autorise pas à fabriquer un bloc interne contigu de 16/24/32/40 Kio. L’ELF grande pile utilise déjà l’API `xTaskCreatePinnedToCoreWithCaps`; cela n’autorise pas de déplacer sans analyse toutes les piles et callbacks système.
- `heap_caps_malloc_extmem_enable(0)` dirige déjà les allocations génériques compatibles vers PSRAM. LVGL demande explicitement PSRAM. TLS est installé une fois au démarrage, pas modifié à chaque requête. Le port NimBLE a un allocateur PSRAM strictement lié au SDK audité ; la consommation du host/transport reste distincte de la mémoire radio C6.
- Le gestionnaire conserve des réserves futures au démarrage et les décrémente après allocation effective pour éviter le double comptage. La marge interne 32 Kio et PSRAM 512 Kio ne doivent pas être supprimées pour masquer un refus. Les refus distinguent total libre, bloc interne, bloc DMA, PSRAM et bloc PSRAM. Les budgets sont des estimations conservatrices, **pas des mesures de consommation physique**.
- Quitter Cartes invalide la génération, libère les résultats en file et les tuiles RAM, ferme le pack hors ligne et réveille le worker qui détruit HTTP/TLS avant sa sortie. L’UI libère son canvas. Les petites tables de configuration/recherche restent en PSRAM ; elles ne démontrent pas une fuite SRAM. Une difficulté à lancer la radio immédiatement après sortie doit être corrélée au retour effectif du stack FreeRTOS et aux admissions en cours.

La mémoire statique liée de référence 0.8.45 est 93 268 octets data+BSS, IRAM 98 494 octets, TCM 794 code + 64 données, RTC noinit 1 200 octets. Ces sections ne décrivent pas le heap au repos et ne suffisent pas à calculer le pic d’une application. Voir le bilan final pour les sections réellement reconstruites en 0.8.46.

## Vérifications matérielles nécessaires

Avant ouverture Cartes, juste après sa fermeture et après démarrage radio : relever la commande série `m`, les refus `[MEM][REFUS]`, les fins de tâches `[STACK]` et l’état tâches depuis le diagnostic existant. Conserver le plus grand bloc, pas seulement le total libre. Dix cycles identiques permettent de distinguer rétention stable, retour différé par idle et croissance persistante.

Pour les gels : activer le profilage UI existant, reproduire une action unique, puis relever `d`. Les durées de phases sont des temps écoulés, pas un pourcentage CPU. Pour un flash au repos, comparer les compteurs DSI et publications avant/après ; le balayage de la dalle continue même si aucune invalidation LVGL n’a lieu. Une boucle de sondage n’implique pas une réinitialisation ou un rafraîchissement inutile.

Aucun watchdog n’a été désactivé dans cet audit, aucune pile n’a été réduite sur estimation, aucun transfert en cours n’a été libéré de force, aucune sauvegarde forcée à l’extinction n’a été ajoutée.
