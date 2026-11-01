# Revue indépendante des contrats et de la validation — 0.8.49

Cette revue prend pour état d'entrée le code 0.8.48 copié dans le chantier
0.8.49 et les symptômes matériels encore présents signalés par l'utilisateur.
Elle ne certifie pas la disparition des flashs ni des claquements. Les tests
hôtes existants prouvent certaines propriétés de leurs composants ; les
retours matériels empêchent de considérer l'ensemble du système comme validé.

## Faits, inférences et contre-exemples

| Sujet | Établi par les sources ou le binaire | Limite de la conclusion |
| --- | --- | --- |
| Admission mémoire | `tab5MemorySample(Budget)` appelle le plus grand bloc pour les budgets qui le demandent. Dans le SDK livré, cette interrogation parcourt tous les blocs de chaque heap correspondant sous verrou critique. | La durée réelle sur P4 et la corrélation avec un flash restent à mesurer. Le parcours est une cause plausible commune à plusieurs applications, pas une preuve exclusive. |
| Fin audio/vidéo | À l'entrée de l'audit, `release(client)` pouvait quitter silencieusement après 250 ms sans obtenir le verrou, alors que l'appelant achevait son nettoyage. | Ce contrat est incorrect même sans reproduire un flash. Il faut vérifier l'absence de références PCM restantes avant de libérer ou de publier l'état arrêté. |
| Cycle audio historique | La version FullPlayer 0.4.10 ne détruisait pas systématiquement le transport I²S à chaque arrêt. La version récente le détruit dans les transitions stop/pause. | La différence est réelle ; elle ne prouve pas seule l'origine analogique des claquements. La version historique n'est pas sûre à recopier telle quelle : son arrêt ne confirmait pas la fin de consommation des pointeurs PCM. |
| Caches et données vivantes | Un catalogue reconstructible, un document modifié et une image encore référencée par LVGL ne peuvent pas suivre la même politique de libération. Plusieurs refus de collecte sont liés à la visibilité de toute la page. | Conserver intentionnellement un cache n'est pas une fuite ; conserver trois images vidéo quand une seule est référencée réclame un examen du propriétaire de chacune. |
| Buffers LVGL historiques | Le code FullPlayer contient une branche « buffers internes », mais `TAB5_LVGL_INTERNAL_BUFFERS` vaut 0 par défaut. Le chemin actif utilise la PSRAM. | L'affirmation « tout marchait auparavant grâce aux buffers internes » est réfutée par la configuration par défaut inspectée. |
| Fin de transaction M5 | `M5.Display.endWrite()` atteint `Panel_FrameBufferBase::display`, qui appelle `Cache_WriteBack_Addr`. Ce chemin ne publie pas directement un nouveau framebuffer DMA. | Aucun double basculement prématuré n'a été trouvé sur ce chemin. Cette absence ne prouve pas toute la synchronisation du système. |
| Événements DMA/VSYNC | Le callback DMA appelle `on_refresh_done` sous `#if !MIPI_DSI_BRG_LL_EVENT_VSYNC`. La constante VSYNC vaut 0 avant la révision 3. | Le soupçon d'un double comptage systématique DMA+VSYNC est écarté pour le SDK et la configuration examinés. Les deux variantes doivent néanmoins rester testées. |

## Pourquoi les bancs précédents n'étaient pas suffisants

Les tests mémoire 0833 exécutent le gestionnaire et certaines fonctions de
l'interface, mais leur échantillon de heap renvoie directement les valeurs
`freeInternal` et `freePsram`, y compris pour les plus grands blocs. Les tests
de transport audio 0845–0847 renvoient un plus grand bloc constant. Un test
ainsi construit ne peut pas détecter le coût d'un parcours TLSF ni son effet
sur les interruptions.

Les bancs de pixels, d'orientation, de miroir ou d'undo vérifient utilement
leurs sorties et leurs limites mémoire. Ils ne prouvent pas qu'un framebuffer
PSRAM est visible du DMA au bon moment. Une copie dans un unique tableau
hôte ne représente pas les états distincts CPU/cache/mémoire externe/DMA.

Les tests audio avec un `Speaker` simplifié qui copie immédiatement les
échantillons éliminent la durée de vie des pointeurs réellement consommés
par la tâche de mixage. Un test du mixeur seul ne vérifie pas non plus que
l'application attend la restitution du périphérique avant de disparaître.

Ces limites doivent accompagner les résultats ; elles ne rendent pas les
tests existants inutiles. Les nouveaux tests doivent couvrir les contrats
entre ces éléments, sans encoder directement la correction attendue dans
leurs doubles.

## Preuve reproductible du parcours de heap

Le répertoire `heap-walk/` conserve les sources Espressif IDF 5.5.4 et le
sous-module TLSF épinglé, leurs empreintes, le programme exécuté et ses
résultats. Il exécute le véritable `multi_heap_get_info_impl`, remplace
uniquement les primitives de verrou de plateforme par une instrumentation
hôte et observe chaque visite du walker au link.

Pour 100, 1 000, 10 000 et 100 000 allocations, avec libération d'un bloc sur
deux, le parcours visite respectivement 101, 1 001, 10 001 et 100 001 blocs,
tous sous verrou. La lecture des compteurs libres ne parcourt aucun bloc.
Les durées enregistrées sont des durées hôtes et ne sont pas des mesures
ESP32-P4. Le banc utilise une arène hôte 64 Mio et une ABI 64 bits ; les
raisons et limites sont documentées dans son README.

Le désassemblage de `libheap.a` réellement utilisé, dans
`esp32p4_es-libs/3.3.9`, confirme séparément cette chaîne :

`heap_caps_get_largest_free_block → heap_caps_get_info →
multi_heap_get_info_impl → multi_heap_internal_lock →
xPortEnterCriticalTimeout(-1) → tlsf_walk_pool → unlock`.

Un comptage de mémoire libre est un compteur de capacité ; il ne garantit
pas qu'un bloc contigu peut être alloué. Si l'admission utilise ces compteurs
pour éviter le parcours, le véritable allocateur avec les capacités exactes
doit rester l'arbitre final. Un échec doit nettoyer les allocations déjà
réalisées et rendre la réservation sans publier de faux succès. Le test
doit explicitement inclure « total suffisant, aucun bloc contigu suffisant ».

La correction 0.8.49 conserve ce contrôle de fragmentation avec une requête
TLSF non mutante, bornée par le nombre de heaps, pour le SDK dont les
empreintes ont été vérifiées. Elle réutilise la recherche par bitmaps et
les verrous natifs. `Fit`/`NoFit` répondent à une taille demandée et ne sont
pas présentés comme des maxima mesurés. Les configurations non reconnues
gardent la requête publique ESP-IDF. Le banc
`memory-admission-tests-0849` compare cette requête à l'allocateur réel et
exécute la récupération de fragmentation du gestionnaire. Ses limites ABI
et instrumentation sont détaillées dans son README.

## Contrats d'intégration à exercer

| Parcours réel à conserver dans le test | Perturbation à injecter à la frontière matérielle | Invariants observables |
| --- | --- | --- |
| Application audio → nettoyage → pilote → mixeur → tâche | Mutex de routage occupé plus longtemps que son timeout ; dernière référence PCM encore lue ; nouvelle commande concurrente | Pas d'état Idle ni d'autre propriétaire tant que les anciennes références subsistent. Aucun use-after-free. Fin différée puis restitution effective, sans attente active infinie. |
| Vidéo → stop → image LVGL → admission suivante | Stop au milieu du décodage ; image finale attachée ; seconde vidéo de dimensions différentes ; échec de chaque allocation | Seul le framebuffer effectivement référencé reste épinglé. Décodage terminé avant libération. Erreur récupérable et réservation restituée. |
| Admission → allocateur réel → nettoyage du demandeur | Heap fragmenté malgré un total libre élevé ; échec des allocations intermédiaires ; concurrence avec un autre propriétaire | Les comptes ne remplacent pas malloc. Pas de publication de pointeur partiellement construit. Aucun parcours de heap caché dans les chemins interactifs visés. |
| LVGL flush → blit/miroir → cache → publication DPI → callback natif | Interruption avant, pendant et après publication ; échec de cache ; transfert en cours ; variantes pré-3 et révision 3 | Aucun écrit CPU dans un tampon toujours détenu par DMA. Les invalidations retenues sont rejouées. La publication intervient après visibilité des pixels et du miroir. |
| Carte → cache tuiles → stockage → fermeture → média | Zoom pendant chargement ; annulation pendant fichier ouvert ; radio active puis arrêt ; passage direct à un stream vidéo | Un changement de génération n'invalide pas une transaction déjà propriétaire du fichier. La ressource est rendue après terminaison réelle. La disponibilité Wi-Fi reste distincte d'un verrou de session HTTP. |

Pour l'affichage, un modèle d'intégration doit distinguer une image visible
du CPU et une image visible du DMA ; seul l'appel de cache valide met à jour
la seconde. La logique des callbacks doit provenir du SDK épinglé. Il ne
faut pas remplacer toute la présentation par un `flush_ready()` immédiat.

Pour l'audio, il faut maintenir les références de buffers jusqu'à la
consommation par le mixeur. Les écritures I²C/I²S peuvent être des frontières
instrumentées, mais leur ordre et leurs échecs ne doivent pas être supposés
réussis. Le test logiciel ne mesure ni une tension de sortie ni un bruit
analogique ; le contrôle auditif reste matériel.

## Mesures matérielles minimales

Utiliser le même appareil, la même révision, la même SD et les mêmes fichiers
pour la référence et le candidat. Conserver le firmware de référence. Ne pas
effacer les paramètres et ne pas reformater les supports pour ces essais.

1. Au repos, relever l'état mémoire et les compteurs affichage une fois,
   laisser fonctionner trois minutes, puis relever une seconde fois.
   Observer si un flash coïncide avec un underrun natif, un retard de refresh
   ou une opération système. Éviter les interrogations de plus grand bloc
   répétées pendant l'observation : elles perturbent le système mesuré.
2. Dessin : un clic, un trait continu, undo et redo ; mêmes dimensions et
   même nombre de calques. Relever les durées maximales des opérations et
   les compteurs de présentation avant/après, sans log dans la boucle pixel.
3. Média : effectuer dix cycles MP3 → FLAC → pause → reprise → stop, puis
   deux streams vidéo successifs. Relever propriétaire audio, tâches encore
   actives, mémoire libre par capacité et buffers encore référencés après
   confirmation de fin. Comparer avec les ressources intentionnellement
   conservées ; un delta de cache ne doit pas être étiqueté automatiquement
   « fuite ».
4. Carte puis média : zoomer pendant les chargements, quitter, démarrer la
   radio, l'arrêter, puis lancer deux vidéos. Un défaut doit laisser le code
   d'erreur initial, le propriétaire et les étapes de nettoyage observables.

Une mesure ponctuelle des interruptions masquées et des latences DMA doit
être produite par des compteurs en RAM, avec extraction après la séquence.
Déplacer un parcours de heap vers le deuxième cœur ne suffit pas : le
verrou du heap reste partagé, et les accès PSRAM utilisent la même mémoire
externe. L'affinité doit suivre une analyse des tâches et des interruptions,
puis une mesure ; elle n'est pas une correction automatique.

## Critère de décision

Une correction est acceptée si elle supprime une violation démontrée,
préserve les comportements utilisateur et passe les tests d'intégration
pertinents ainsi que la compilation complète. Les symptômes physiques
restent ouverts jusqu'à l'essai sur Tab5. Un résultat de composant ne doit
plus être présenté comme la preuve de fluidité ou d'absence de flash de
l'ensemble de l'OS.
