# Audit initial — propriété mémoire et cycle de vie

État audité : sources v0.8.49 issues de l'archive v0.8.48 fournie, avant correction de production de cet audit. Lecture du code réel ; aucun résultat sur Tab5 physique revendiqué. Les tailles ci-dessous sont des tailles explicites du code, pas des mesures du tas en fonctionnement.

## Conclusion technique

Le constat « aucun cache n'est vidé » doit être décomposé. Les codecs audio, les buffers radio et le décodeur/entrée vidéo disposent de chemins de destruction réels. Plusieurs rétentions sont intentionnelles (pause locale, image vidéo finale, catalogues, document Dessin). En revanche, le contrat commun de libération audio peut échouer silencieusement, et la politique globale de collecte protège des ressources rechargeables trop largement. L'admission mémoire ajoute aussi des parcours du tas au chemin critique de plusieurs applications ; leur coût doit être mesuré sur cible.

### Défaut établi : libération audio non acquittée

`Tab5AudioDriver.cpp::release()` retourne sans indication d'échec lorsque `_ownerMutex` n'est pas acquis en 250 ms. `service()` et `refreshHeadphoneRouting()` détiennent ce verrou pendant les lectures/écritures I²C de routage casque. Le cas d'un verrou encore occupé est donc un chemin prévu, pas une impossibilité logique.

Les clients divergent :

| Appelant réel | Après un échec de `release()` |
| --- | --- |
| `Tab5AudioEngine::cleanupDecoder()` | Continue, ferme le bail réseau, puis peut publier Idle et retirer son worker. |
| `Tab5VideoEngine::releasePlayback()` | Continue, remet l'horloge à zéro, puis retire le worker. |
| Arrêt alarme | Continue. |
| Worker ELF / SDR | Réessaie explicitement tant que `owner()` lui appartient. |

Le commentaire ELF décrit précisément ce risque ; sa protection n'a pas été intégrée au contrat commun. Le propriétaire et les **12 288 octets PSRAM** du buffer PCM peuvent rester retenus après la fin audio/vidéo. Pour Audio, `cleanupDecoder()` appelle d'abord `AudioOutputTab5Speaker::stop()`, qui joint le worker M5 : la rétention I²S/DMA n'est donc pas déduite dans ce chemin. Pour Video, il n'existe pas cet arrêt séparé ; le timeout peut laisser aussi la sortie I²S active. Le client suivant peut être refusé parce que le propriétaire précédent reste enregistré. `canRetireAudioTask()` ne vérifie pas le propriétaire du pilote. `serviceMemoryPriority()` acquitte sa suspension après `cleanupDecoder()` sans cette vérification non plus.

Gravité : élevée pour les changements de média ; occurrence matérielle à mesurer. Corriger le contrat du pilote et le moment de publication de la libération, sans ajouter des boucles différentes dans chaque application ni supprimer une ressource encore lue.

### Politique établie : cadres vidéo conservés après fin

Le worker libère entrée JPEG et moteur JPEG à sa sortie. Si l'image LVGL est encore attachée, les trois frames restent allouées. À 1280 × 720 RGB565 : **3 × 1 843 200 = 5 529 600 octets**. Conserver l'image finale est justifié ; conserver les deux autres frames après l'arrêt est un coût évitable de **3 686 400 octets**, sous réserve de ne pas libérer une publication encore empruntée par LVGL.

`reclaimIdle()` refuse tout trim tant que `_displayAttached` est vrai. Le bouton de fermeture réel appelle bien `lv_image_set_src(..., nullptr)`, puis `detachDisplay()` ; ce dernier libère tout si le worker est déjà arrêté, sinon le worker le fait après sa dernière lecture. Il ne faut donc pas présenter une fuite générale à la fermeture comme établie.

Attention à un correctif partiel : `ensureDecoderBuffers()` suppose trois buffers présents ou recrée les trois. Libérer deux frames isolément sans adapter ce contrat détruirait potentiellement la frame finale empruntée par LVGL. Toute correction doit couvrir fin, remplacement, échec d'allocation, détachement et publication d'image.

### Politique établie : collecte globale exclusivement sous pression

`Tab5ResourceManager.cpp::collectImpl()` ne collecte que les domaines déficitaires pour l'opération en cours. Il exclut tous les caches du propriétaire foreground et du requester. Les callbacks applicatifs ajoutent leurs propres veto : toutes les listes Audio restent protégées sur la page Média, le catalogue Video dans la vue Vidéos.

Cette protection empêche de détruire un document ou une liste encore référencée, mais la granularité « application » est trop large pour permettre de rendre les caches indépendants d'une autre fonction de la même application. Le commentaire du manager renvoie le trim sûr vers les transitions explicites ; plusieurs transitions ne le font pas. `showPage()` masque la plupart des pages ; quitter Média ne purge pas ses catalogues ou ses lignes. Ce sont des rétentions de politique, pas une perte du pointeur propriétaire.

Le callback `reclaimIdleWorker()` retourne zéro et positionne un drapeau. Le worker audio se retire déjà naturellement dès qu'il est éligible. Les octets de pile ne reviennent qu'après le nettoyage FreeRTOS par idle ; un callback synchrone n'est donc pas une preuve d'achèvement du retrait. Le manager tient compte de la mémoire réellement rendue dans ses compteurs, ce qui est correct, mais ne distingue pas « ressources applicatives libérées » de « pile en attente de récupération ».

### Hypothèse transversale à instrumenter : admission et parcours du tas

`Tab5ImageEditor::allocate()` appelle `tab5ReserveMemory(..., psramBlock=total)` pour chaque allocation du modèle, de l'historique et de l'aperçu. `Manager::admit()` lit alors `heap_caps_get_largest_free_block(PSRAM)`. Un début de pinceau réserve couverture, état d'historique et pixels d'historique : plusieurs parcours de tas précèdent le geste. Les démarrages audio/vidéo passent par le même manager.

Le code `Tab5Memory.h/.cpp` reconnaît déjà que ces parcours prennent les verrous du tas, avec interruptions masquées. L'audit matériel parallèle a confirmé dans le binaire SDK réellement utilisé le chemin `multi_heap_get_info_impl` → lock → `tlsf_walk_pool` → unlock, avec `multi_heap_internal_lock` → `xPortEnterCriticalTimeout`. Il a aussi trouvé `tlsf_realloc` et sa copie sous le même verrou critique. Les appels directs sont centraux : `lv_realloc_core`, `Tab5ByteBuffer`, `Tab5PsramTable`, listes Média, modèle de répertoire, JSON et buffers shell. Une croissance qui déplace un grand buffer copie donc sous interruptions masquées ; c'est une propriété du SDK identifié, pas seulement un commentaire applicatif.

La politique `heap_caps_malloc_extmem_enable(0)` place aussi petits objets et chaînes génériques en PSRAM ; elle peut augmenter le nombre de blocs à parcourir. Le lien avec les flashs reste une hypothèse matérielle à corréler : mesurer durée maximale d'admission/realloc et compteurs DSI/présentation. Déplacer les copies hors verrou nécessite de préserver exactement la propriété de l'ancien bloc, sa taille, l'alignement et le comportement sur échec ; remplacer chaque appel indépendamment sans contrat commun créerait de nouvelles différences de comportement.

### Incohérence établie : admission radio et ring adaptatif

`radioBudget()` impose 2 Mio PSRAM et un bloc contigu de 1 Mio avant l'ouverture. `Tab5BufferedRadioSource::open()` adapte ensuite le ring entre 64 Kio et 1 Mio, notamment à la moitié du plus grand bloc, et réessaie plus petit en cas d'échec. Le contrôle préalable peut donc rejeter un état où le repli implémenté aurait pu fonctionner. Ce n'est pas une preuve de faisabilité de toute lecture avec 64 Kio : codecs, TLS et marge doivent encore être comptés. La correction cohérente consiste à réserver le ring réellement choisi et les workspaces réels, pas à diminuer une constante isolée.

Tous les budgets, y compris ceux uniquement PSRAM, imposent aussi la marge interne globale de 32 Kio plus les démarrages en cours. Une opération peut ainsi être refusée malgré de la PSRAM disponible. Cette marge protège le système ; les diagnostics doivent différencier cette réserve globale du besoin interne propre à l'application.

## Carte des propriétaires et libérations

| Ressource | Allocation / mémoire | Arrêt, fin et fermeture réels | Rétention restante |
| --- | --- | --- | --- |
| MP3 | Préallocation `AudioGeneratorMP3::preAllocSize()` en PSRAM + objet codec | `cleanupDecoder()` stop/delete codec, free arena, close/delete source | Pause locale garde codec/source pour reprendre ; arrêt les libère. |
| AAC | Une arène PSRAM, taille calculée avec Helix/SBR + codec | Même destruction explicite | Pause locale intentionnelle. |
| FLAC/WAV | Objets bibliothèques, allocations génériques | stop/delete + fermeture source | Capacités internes de bibliothèque à confirmer par trace du tas, pas de pointeur moteur conservé après cleanup. |
| Radio | Ring PSRAM 64 Kio à 1 Mio ; lecture 16 Kio ; pile réseau interne 16 Kio ; HTTP/TLS | `close()` demande annulation, attend le receiver, ferme transport, détruit ring, libère readBuffer et sémaphore | Attente réseau jusqu'à retour d'une lecture ; ne pas forcer free avant l'acquittement. La pile revient ensuite via idle. |
| Sortie audio partagée | PCM PSRAM 12 Kio ; M5/I²S/DMA internes selon pilote | `stop()` joint M5 ; `release()` est le défaut détaillé plus haut | Mutex et objet sortie permanents ; PCM ne doit pas rester après libération. |
| Enregistrement | 3 × 1024 × 2 = 6144 octets PSRAM, fichier WAV | `finishRecording()` ferme le fichier et libère le buffer | Aucune autosauvegarde nouvelle proposée. |
| Catalogues audio | Vecteurs de chemins/stations + chaînes ; file de commandes PSRAM 12 × sizeof(command) | Catalogues libérés par `releaseIdleCatalogues()` sous pression quand sûrs | Index actifs préservés ; file et mutex sont des services permanents. |
| Vidéo | Trois frames PSRAM alignées ; entrée JPEG PSRAM ; moteur JPEG ; pile interne 24 Kio | Décodeur/entrée libérés dans le worker, frames au détachement ou fin si cachées | Trois frames en affichage final ; catalogue rechargeable conservé jusqu'à collecte. |
| Cartes | Canvas UI ; tuiles compressées max 2 Mio ; tables/queues PSRAM ; pile réseau interne 16 Kio | `setActive(false)` invalide génération, draine résultats, ferme pack, vide cache tuiles ; UI libère canvas ; worker ferme socket et se retire | Tables styles/lieux/packs, contrôles de queues et quarantaine restent services permanents. Aucun leak général établi. |
| Dessin | Modèle RGBA, sélection, undo/redo PSRAM ; aperçu RGB565 dérivé | `show(false)` libère aperçu ; `close()` autorisé détruit modèle et historique | Document et undo/redo conservés lorsqu'on masque l'app : données utilisateur, pas cache jetable. Historique borné à 8 Mio et 16 entrées. |
| MicroPython | VM 2 Mio PSRAM + source ; pile interne 32 Kio | Worker libère VM/source et services après retour VM, puis publie inactif | Console PSRAM 8192 octets conservée après exécution, sans callback de collecte vu. Elle contient la sortie utilisateur. |
| ELF | Image relogée, contexte, allocations suivies, fichiers, canvas/panel ; pile interne 16 Kio ou stack externe demandée | Worker sort/relâche matériel ; UI cleanup ferme fichiers, abort transaction en cours, free allocations, `esp_elf_deinit`, free contexte | Console + snapshot 2 × 8192 octets rechargeables via callback. Pas d'isolation mémoire matérielle déduite. |

Les files/mutex de services sont normalement conservés entre ouvertures pour éviter une recréation inutile. Leur présence ne prouve pas une fuite. Inversement, afficher « Idle » n'est pas une preuve qu'un pilote a rendu son ownership ni qu'idle a récupéré la pile FreeRTOS.

## Répartition mémoire et tâches

La séparation actuelle `Tab5MemoryLayout` compte les heaps réellement enregistrés : INTERNAL agrégé ; HP SRAM interne DMA ; SPM/TCM via `MALLOC_CAP_TCM` ; LP via `MALLOC_CAP_RTCRAM` ; PSRAM séparée. INTERNAL ne doit pas être additionné aux trois sous-pools. Les 768 Kio HP physiques ne sont pas le heap libre : cache L2, code et données statiques en retirent une partie. Aucun transfert arbitraire de DMA, stacks ordinaires ou état ISR vers PSRAM n'est proposé.

La politique générique `extmem_enable(0)` est appliquée avant M5/USB/réseau. Le commentaire affirmant que tous les pilotes demandent déjà correctement INTERNAL/DMA est trop absolu : cette propriété dépend du code exact des bibliothèques. Le précédent correctif DSI lui-même prouve la nécessité de vérifier chaque contexte ISR/périphérique. Revenir globalement à une préférence interne sans budget des milliers de chaînes risquerait en revanche d'épuiser SRAM. Préférer des capacités explicites aux frontières matériel/ISR et des allocations PSRAM explicites pour les gros modèles.

Le second cœur est déjà utilisé : Audio décode sur l'autre cœur que l'UI (priorité 5), M5 Speaker aussi (6), réception radio sur cœur UI (4), Vidéo sur autre cœur (5), Cartes réseau sur cœur UI (2). Les piles d'Audio, Radio, Carte font chacune 16 Kio, Vidéo 24 Kio, MicroPython 32 Kio. Le code ne manque donc pas d'une simple affectation au second cœur ; il faut mesurer contention PSRAM, verrous et délais de consommation. Ajouter un worker ne supprime pas une limite de bande passante mémoire ni un verrou global.

## Axes de correction proposés, avant implémentation

1. **Contrat commun audio de libération acquittée.** Un échec de verrou ne doit pas être annoncé comme un arrêt complet. Une seule implémentation commune doit séparer possession, arrêt effectif du consommateur et retour des buffers. Tester tous les clients et le prêt audio pour vidéo/impression.
2. **Séparer les durées de vie par propriété.** Pour chaque app : travail actif, données utilisateur à conserver, ressources empruntées par UI, caches rechargeables, services permanents. Rendre les buffers de travail à la fin réelle ; conserver seulement la frame empruntée ; ne pas vider undo/redo ou pause locale pour gagner artificiellement de la mémoire.
3. **Éviter les diagnostics coûteux dans le chemin critique.** Instrumenter les échantillons/admissions. Les compteurs d'octets et les réservations doivent rester rapides ; garder les parcours détaillés pour refus réel/diagnostic, en conservant une gestion sûre d'un malloc qui échoue. Ne pas prétendre qu'une pré-vérification garantit une allocation concurrente.
4. **Transitions explicites, pas timer de purge.** Les caches sans emprunteur peuvent être rendus à hide/finish/close ; le collecteur sous pression reste un filet ciblé. Le manager ne doit pas devenir un destructeur général d'applications ou un second ordonnanceur.

## Validation requise

Non exécuté à cette étape : compilation modifiée, test physique, preuve de fuite sur cycles matériels, mesure de flash. Ce document est une base d'audit, pas une déclaration de correction.

Tests hôte à construire sur les véritables points d'entrée :

- Driver réel avec mutex de routage indisponible : cleanup Audio / release Video ne doivent pas déclarer libéré avant owner=None ; vérifier PCM, M5 et acquittement priorité. Inclure démarrage refusé et changement de client.
- `playSource()` → worker → publication → fin → seconde source → détachement ; faux JPEG/RTOS seulement aux frontières matérielles. Compter allocations libérées, protéger l'adresse réellement fournie à l'image, échecs injectés après chaque allocation. Ne pas tester uniquement une nouvelle fonction de trim isolée.
- Annulation Radio pendant HTTP/TLS / lecture / préchargement : destruction après dernier accès receiver ; pas de socket ou ring conservé au retour cleanup.
- Cycles Cartes hide/reopen et dessin hide/reopen/close : tuiles/aperçus reviennent, modèle et historique demeurent inchangés tant que l'utilisateur ne ferme pas.

Sur cible, capturer `memory` et `tasks` aux points suivants : avant lecture ; lecture ; pause ; stop acquitté ; après un tour idle ; après dix cycles ; après fermeture viewer. Ajouter un instantané peu coûteux des octets détenus par classe et des états released/retiring ; pas de parcours permanent du tas. Différencier une mémoire stable gardée intentionnellement d'une pente par cycle et d'un retour retardé. Reproduire navigation Média et clic Dessin avec corrélation temporelle des admissions/flush/DSI et comparer la version réellement fonctionnelle, sans augmenter le logging dans les ISR.
