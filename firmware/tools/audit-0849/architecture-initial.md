# Audit architectural initial — Tab5OS 0.8.49, état repris de 0.8.48

Analyse des sources réelles, avant corrections de cet ensemble. Les noms de fonctions ci-dessous identifient les chemins exécutés ; les constats ne reposent pas sur les anciens comptes rendus. Aucun essai physique n'a été réalisé dans cet audit. Le simple emploi de tâches, d'événements ou de PSRAM ne démontre ni ne réfute une cause de flash.

## Architecture exécutée et propriété

Le firmware conserve une tâche Arduino/UI, une collection de moteurs natifs et des travaux FreeRTOS démarrés à la demande. `setup()` crée la majorité des pages LVGL, initialise les services, puis enregistre des récupérateurs de caches. `showPage()` masque essentiellement les pages existantes ; ce n'est pas une destruction d'application. Les documents, préférences et services d'arrière-plan doivent donc être distingués des caches rechargeables.

`loop()` constitue le propriétaire LVGL : entrées M5/tactile/souris/clavier → service des demandes de récupération mémoire → audio/vidéo/persistance média → garde du rafraîchissement → `lv_timer_handler()` → services cartes/fichiers/éditeur/Python/ELF/mesh → widgets modifiés → persistance selon la politique utilisateur. Les services sont appelés à chaque tour mais beaucoup possèdent déjà des révisions, indicateurs et files ; leur présence dans cette boucle ne signifie pas une réécriture permanente de l'écran. Le sommeil final est borné à 8 ms écran actif, 30 ms éteint. Aucun `lv_refr_now()` de production n'a été trouvé.

| Chaîne | Contexte réel | Propriété et contraintes |
|---|---|---|
| LVGL, pages, pointeur, listes | Tâche Arduino/UI, pile 24 Kio | Un seul propriétaire des objets. Les callbacks de récupération qui détruisent des objets s'exécutent dans cette tâche. |
| Décodeur MP3/AAC/FLAC/WAV | Tâche audio à la demande, pile 16 Kio, priorité 5, cœur opposé à UI | Commandes en file ; le propriétaire ferme lui-même fichiers, sockets et décodeurs. |
| Sortie PCM M5Unified | Priorité 6, même cœur que décodeur | Le pilote est partagé par clients exclusifs. `release()` joint le lecteur avant de libérer le tampon PCM. |
| Réception radio | Pile 16 Kio, priorité 4, cœur UI | Attend la socket/stream buffer. Une priorité élevée peut préempter UI lors du traitement effectif ; aucune starvation n'est établie sans mesures. |
| Vidéo | Pile 24 Kio, priorité 5, cœur opposé à UI | Le même worker consomme une nouvelle lecture mise en attente. La vidéo emprunte la priorité audio et rend les ressources avant restitution. |
| Réseau cartes | Pile 16 Kio, priorité 2, cœur UI | Files et générations annulent les demandes obsolètes ; fermeture/réactivation coordonnées par mutex de cycle de vie. |
| MicroPython | Pile 32 Kio, priorité 1, cœur 0 | VM en PSRAM ; coopère par yield. La pile normale reste une allocation interne. |
| ELF | Worker priorité 2, cœur 0 | Selon taille, pile interne du lancement ou `xTaskCreatePinnedToCoreWithCaps` en PSRAM. Même espace mémoire que le firmware : aucune isolation matérielle démontrée. |
| Persistance du navigateur média | Pile 6 Kio, priorité 1, affinité libre | Copie immuable détenue par worker, résultat publié atomiquement ; UI ne rejoint pas le worker. |

Le deuxième cœur est donc déjà utilisé. Déplacer arbitrairement tous les services sur ce cœur ferait notamment concurrence au décodeur et au haut-parleur ; la décision exige temps d'exécution/réactivité observés et non un comptage de tâches.

## Constats confirmés

### A. Le contrat « application masquée / cache libérable » reste incomplet

Chemin : `showPage(Audio → autre)` → mémorisation de position et `Tab5MediaSession::flush()` → masquage de la page. Aucune demande de retrait des listes/catalogues n'est faite à cette transition.

Chemin de récupération : `tab5ReserveMemory()` → `Manager::reserveRecovering()` → `collectImpl()` **uniquement si un budget manque** → exclusion du propriétaire visible et du demandeur → callbacks `registerResourceCaches()`.

Les listes et catalogues Media sont donc conservés jusqu'à une pression ultérieure et protégés pendant que Media est visible. Ceci explique un écart entre « j'ai arrêté/quitté » et « la mémoire est revenue », mais ne prouve pas une fuite de décodeur. Le commentaire du gestionnaire exige un nettoyage local explicite à fermeture/fin ; ce contrat n'est pas appliqué uniformément par les pages.

Le récupérateur des catalogues audio prend aussi un `Tab5StorageRecovery` exclusif avant le verrou d'état, alors qu'il libère des vecteurs mémoire. Une lecture SD étrangère peut ainsi empêcher cette récupération. Supprimer ce verrou sans remplacer son rôle de synchronisation serait dangereux : la politique de propriété doit être explicite.

### B. Des ressources lourdes sont bien libérées : éviter un faux diagnostic

`Tab5AudioEngine::cleanupDecoder()` arrête/détruit les décodeurs, libère les espaces MP3/AAC, ferme les sources, abandonne le bail réseau et rend `Tab5AudioDevice`. Le pilote `release()` arrête/joint M5Unified puis libère son tampon PCM. La pause locale conserve volontairement l'état de reprise ; la pause radio libère le flux live. Arrêter la musique ne doit pas être confondu avec quitter un catalogue.

`Tab5VideoEngine::taskMain()` ferme entrée réseau/fichier, JPEG et sortie audio. `detachDisplay()` libère les trois images si le worker a fini ; sinon `taskEntry()` les libère après fermeture quand l'écran est détaché. L'image finale reste volontairement détenue tant que LVGL l'affiche. Le catalogue et des chaînes de navigation restent récupérables séparément. Une seconde lecture réutilise maintenant le worker actif, au lieu d'exiger immédiatement deux piles de 24 Kio. Il faut mesurer chaque catégorie avant d'annoncer « aucune libération » ou « fuite corrigée ».

### C. Admission mémoire : une voie synchrone bloque encore l'UI sous pression

`Manager::reserveRecovering(collectorThread=true)` peut parcourir les récupérateurs, puis répéter l'admission avec `vTaskDelay(1)` pendant jusqu'à 60 ms. L'objectif est d'attendre que les tâches idle rendent les piles des workers terminés. Ce délai ne traite pas les entrées/LVGL et peut être atteint depuis un callback de lancement. Déplacer le décodeur sur un autre cœur ne supprime pas ce blocage du demandeur.

Les admissions sollicitent aussi le plus grand bloc pour chaque domaine demandé. Une répétition d'admissions pendant cette fenêtre peut donc répéter le parcours des tas. Il ne faut pas remplacer ce mécanisme par une attente plus longue ; un démarrage différé, conduit par un état explicite, est le contrat à viser pour les opérations UI qui rencontrent réellement cette pression.

### D. Les premières ouvertures n'ont pas toutes un modèle publié de façon asynchrone

`Tab5AudioEngine::ensureLibraryLoaded()` peut appeler l'énumération de fichiers et le chargement des stations synchroniquement ; `Tab5VideoEngine::ensureLibraryLoaded()` appelle `reload()`. Les callbacks de construction de Media peuvent donc effectuer des accès SD avant de rendre la main à LVGL. Cela explique une latence possible d'ouverture, pas un flash de DSI établi. Recharger moins souvent et réutiliser une liste ne justifie pas un accès bloquant sans borne dans UI.

### E. Politique globale de mémoire agressive, mais causalité d'affichage non établie

`tab5ResourcesBegin()` appelle `heap_caps_malloc_extmem_enable(0)` avant M5Unified/USB/réseau. Cela préfère la PSRAM même pour de petites allocations génériques. `Tab5LvglMemory.cpp` place les objets LVGL dans le tas PSRAM partagé ; le tampon de dessin est également en PSRAM (1 × 32 lignes, 80 Kio). Les piles normales, allocations explicitement INTERNAL et DMA conservent leurs contraintes distinctes. La PSRAM libre ne garantit donc jamais l'admission d'une pile interne ou d'un bloc DMA.

La base fonctionnelle historique 0.4.10 employait **par défaut deux tampons LVGL de 52 lignes en PSRAM** ; son option de tampons SRAM était désactivée. Il serait erroné d'attribuer la régression à un « passage des tampons de SRAM à PSRAM » sur cette seule comparaison. Les changements réellement comparables concernent aussi le tas LVGL partagé, la politique globale d'allocation, le pipeline de présentation et les dépendances compilées.

### F. Instrumentation peu coûteuse au repos, un parcours de tas encore périodique en radio

Les commandes série `m`, `p`, `d` permettent mémoire/tâches, délais UI et DSI. L'aperçu Informations système n'est rafraîchi que si ouvert ; les détails de fragmentation sont demandés explicitement. Aucune boucle de diagnostic de fragmentation PSRAM au repos de l'accueil n'a été trouvée.

Cependant `TAB5_RADIO_TRACE` vaut 1 par défaut : `serviceTelemetry()` appelle toutes les 5 secondes, pendant la lecture radio, `heap_caps_get_largest_free_block(INTERNAL)` et les marges de deux piles. C'est une surcharge de diagnostic établie ; un lien avec les flashs nécessite la durée et la corrélation DSI. Les admissions et journaux de début/fin effectuent d'autres mesures ponctuelles.

## Comparaison effective 0.8.47 → 0.8.48

Les fichiers `Tab5ResourceManager.cpp`, `Tab5Memory.cpp`, `Tab5LvglMemory.cpp`, `Tab5Task.h`, `Tab5AudioDriver.cpp` et `Tab5AudioWorkerLifecycle.h` sont strictement identiques dans les deux sources extraites. Les politiques globales d'allocation, le mécanisme d'admission et les limites du cycle de vie décrites ci-dessus n'ont donc pas été remaniés par la dernière livraison. La vidéo a réellement changé (réutilisation du worker pour un remplacement, allocation/JPEG et sondage d'en-tête) ; d'autres correctifs du son sont dans les dépendances M5Unified préparées par la compilation et doivent être vérifiés dans ces fichiers effectivement compilés. Aucune conclusion acoustique ne peut être tirée du seul commentaire de livraison.

## Contrats préservés et limites

- LVGL demeure dans la tâche UI. Les services concurrents publient des états/révisions ; aucun appel LVGL concurrent confirmé dans les chaînes audio/vidéo examinées.
- Le bail SD empêche un démontage sous lecteurs, mais n'est pas un mutex sérialisant toutes les transactions. Il ne faut pas ajouter une énorme file globale de fichiers sans connaître le vrai besoin du pilote/VFS.
- La mémoire d'un worker terminé peut rester allouée jusqu'au passage idle FreeRTOS ; son drapeau `stopped` n'est pas la preuve que sa pile est déjà revenue au tas.
- Un remplacement d'UI/cache ne doit jamais détruire un pointeur de frame encore détenu par LVGL, un document modifié, une sélection/undo, un décodeur en pause repris ensuite ou un service d'arrière-plan actif.
- L'arrêt ou l'incident ne doit ajouter aucune sauvegarde forcée. Les réglages, secrets, partitions et fichiers doivent rester intacts.
- La découverte ELF actuelle reconnaît les suffixes `.elf` et les entrées `main.app.elf`/`main.elf` ; aucun blacklistage nominatif de binaires n'a été trouvé dans `Tab5AppCatalogue::stepScanIo()`. Les exclusions de fichiers cachés, la limite d'énumération et les choix d'épinglage existent : ne pas les appeler arbitrairement « artefacts » et effacer les préférences utilisateur.

## Trois ensembles cohérents, à coordonner avec les autres audits

1. **Contrat de fin/masquage et récupération** : identifier et mesurer séparément ressources actives, image encore référencée, cache rechargeable et état utilisateur. Demander le retrait de caches seulement lors des transitions pertinentes, exécuter la destruction UI après la transition et réessayer sur événement de fin de propriétaire. Préserver lecture de fond, pause, sélection et documents. Tester fermeture/reprise/lecture suivante, pas seulement un appel `free()`.
2. **Contrat d'accès temps réel à l'affichage et à l'audio** : vérifier le chemin réellement compilé M5Unified/DSI/I2S contre les versions fonctionnelles ; établir d'abord les violations de cache, d'interruptions et de séquençage de périphériques. Une mémoire intermédiaire ne se justifie qu'avec une contrainte identifiée ; un deuxième cœur ne rend pas sûrs les accès PSRAM ou I²C concurrents.
3. **Contrat UI non bloquant** : supprimer progressivement l'attente de récupération dans les callbacks UI au bénéfice d'un état de démarrage différé ; publier les résultats de scan par génération et garder l'UI propriétaire de ses objets. Aucun nouveau planificateur nécessaire. Mesurer temps de callbacks, latence d'entrée, temps de flush et sous-alimentation DSI autour d'une même interaction.

## Vérification attendue avant conclusion matérielle

Pour chaque scénario, comparer les mêmes fichiers/flux et la même configuration : mémoire libre interne/PSRAM, plus grands blocs sur demande, nombre de tâches, compteur DSI avant/après, temps maximal UI/flush, ressources de lecteur et catalogues. Scénarios minimaux : Media → onglets → accueil ; MP3 puis FLAC puis Stop ; vidéo1 → fermer → vidéo2 ; clic Dessin avec/sans création undo ; repos accueil puis Media sans lecture. La présence d'un flash sans nouveau flush LVGL distingue une panne de transport/bus d'une invalidation graphique ; un flash avec flush ne prouve pas que LVGL en soit la cause.

Cet état initial n'est pas une preuve de correction des symptômes, ni une certification production. L'audit établit des contrats incohérents et écarte des affirmations trop larges ; les mesures physiques et la comparaison des chemins compilés restent nécessaires pour attribuer les flashs et claquements.
