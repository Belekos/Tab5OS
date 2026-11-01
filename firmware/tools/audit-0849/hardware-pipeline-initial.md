# Audit initial : chaîne graphique, mémoire et interruptions

Analyse des sources de 0.8.48 copiées dans le projet 0.8.49, avant correction de ce lot. Bibliothèques réelles de compilation : M5GFX 0.2.30, M5Unified, SDK M5Stack 3.3.9 / ESP-IDF 5.5.4, profil ESP32-P4 avant révision 3. Aucun essai sur tablette n'a été réalisé.

## Résultat principal établi

Le périphérique DSI continue de lire la PSRAM même lorsque LVGL ne redessine rien. La fin de chaque transfert DW-GDMA déclenche `mipi_dsi_dma_trans_done_cb`, qui réarme explicitement le transfert suivant. L'exécution ponctuelle de cette interruption est donc nécessaire à une image stable, y compris au repos.

Le SDK initialise cette interruption pendant `M5.begin()` depuis `setup()`. Le profil natif utilise `CONFIG_ARDUINO_RUNNING_CORE=1`; `esp_intr_alloc` et `esp_intr_alloc_intrstatus` allouent sur le cœur appelant. Le réarmement DSI partage donc le cœur 1 avec la tâche UI. La correction IRAM de 0.8.48 permet l'exécution lorsque le cache Flash est arrêté, mais ne permet pas de contourner une section critique qui masque les interruptions.

Deux chemins généraux du SDK, appelés par la stratégie mémoire de l'OS, masquent justement les interruptions pendant des opérations dont le coût n'est pas constant :

1. `heap_caps_get_largest_free_block` appelle `heap_caps_get_info`, puis `multi_heap_get_info_impl`, qui verrouille le heap, parcourt **tous ses blocs** avec `tlsf_walk_pool`, puis déverrouille. L'OS appelle cette interrogation depuis les admissions mémoire : dessin, création des listes, lancement audio/vidéo, etc. Les allocations LVGL individuelles et les petits objets génériques en PSRAM augmentent le nombre de blocs de ce heap commun.
2. `multi_heap_realloc_impl` maintient le même verrou pendant `tlsf_realloc`. Si le bloc doit être déplacé, ce dernier effectue `memcpy` avant de libérer le verrou. Les buffers généraux, tableaux PSRAM, réallocations LVGL, listes Média et structures JSON utilisent `heap_caps_realloc`. La copie d'un gros bloc peut donc se dérouler avec les interruptions masquées, même si la fonction de l'application n'emploie aucune section critique explicite.

Ces chaînes ont été vérifiées dans **le binaire natif `libheap.a` réellement utilisé**, et non seulement dans une version Internet du SDK. `multi_heap_internal_lock` appelle `xPortEnterCriticalTimeout`; son pendant appelle `vPortExitCriticalMultiCore`. Les désassemblages complets des fonctions concernées et le SHA-256 de l'archive sont dans `sdk-heap-critical-disassembly.txt` et `sdk-heap-critical-proof.json`.

Un troisième chemin existe dans le binaire natif `libesp_mm.a` : `esp_cache_msync` garde `s_spinlock` sous section critique pendant `cache_hal_writeback_addr` et `cache_hal_invalidate_addr`. Le rendu de l'OS borne déjà ses propres appels à 16 Kio; cette protection ne couvre pas les appels internes des pilotes. Le décodeur JPEG natif synchronise en une fois `buffer_left`, puis invalide `outbuf_size` entier avant et après le DMA (`tools/video-tests-0848/sdk/jpeg_decode.c`). L'adaptateur ELF synchronise également son segment entier. Le verrou cache est partagé : un appel sur le cœur 0 peut retarder le cœur 1 qui attend ce verrou dans sa propre section critique. Preuve binaire et hash : `sdk-cache-critical-disassembly.txt` et `sdk-cache-critical-proof.json`. Cette observation impose une analyse des tailles et durées; elle ne justifie pas de remplacer globalement une API SDK sans préserver ses contraintes d'alignement et de cohérence.

Il s'agit d'un défaut architectural démontré : un chemin graphique sensible aux délais d'interruption partage son cœur avec des diagnostics de heap et des copies sous section critique. **La durée réelle et la correspondance avec les flashs constatés restent à mesurer sur la tablette**. Ce constat ne démontre pas que tous les flashs ont une cause unique.

## Différences historiques réellement vérifiées

Le projet 0.4.10 FullPlayer retrouvé utilisait par défaut deux tampons LVGL de 52 lignes en PSRAM. Son option `TAB5_LVGL_INTERNAL_BUFFERS` vaut **0** par défaut. Il serait faux de présenter des tampons internes comme son comportement historique par défaut.

En revanche, son `lv_conf.h` demandait un pool privé LVGL de 2 Mio en PSRAM. La version actuelle passe chaque allocation et réallocation LVGL au heap ESP partagé via `Tab5LvglMemory.cpp`. Elle appelle également `heap_caps_malloc_extmem_enable(0)` avant `M5.begin`, ce qui place préférentiellement les petits objets génériques dans la même PSRAM. Cette différence augmente les interactions entre créations UI, allocations des applications et diagnostics du heap commun. Elle ne justifie pas à elle seule de réintroduire un pool permanent de 2 Mio ni de renvoyer tous les petits objets en SRAM.

Le témoignage de fonctionnement de 0.4.10 concerne l'audio. Les versions exactes de ses dépendances et sa configuration binaire complète ne sont pas disponibles ici. Le diagnostic LVGL animé signalé sans scintillement est un contre-exemple utile, mais son code n'est pas inclus dans cette comparaison initiale.

## Affichage et trafic mémoire

La version actuelle possède deux framebuffers DSI natifs de 720 × 1280 × 2 octets, soit 3 686 400 octets, et un tampon partiel LVGL de 1280 × 32 × 2 octets, soit 81 920 octets en PSRAM. Le rendu utilise une copie transposée par tuiles de 16 × 16 pixels (512 octets de pile interne), puis une synchronisation de cache des régions modifiées. Le buffer envoyé au DSI reste en lecture seule jusqu'à la clôture vérifiée par les événements natifs. Les copies de synchronisation entre les deux images sont différées et amputées des régions totalement redessinées.

La source du pilote fixe les paramètres nominaux suivants :

| Dalle reconnue | Horloge DPI demandée | Trame totale | Fréquence nominale calculée | Pixels actifs RGB565 lus par seconde |
|---|---:|---:|---:|---:|
| ILI9881C | 80 MHz | 940 × 1324 | 64,28 Hz | 118,48 Mo/s |
| ST7121 | 70 MHz | 802 × 1524 | 57,27 Hz | 105,56 Mo/s |
| ST7123 | 80 MHz | 802 × 1510 | 66,06 Hz | 121,76 Mo/s |

Ces valeurs découlent des horloges **demandées** et des porches du code. Elles ne sont pas des mesures de l'horloge réalisée, du débit effectif PSRAM ou des FPS de l'interface. Elles montrent que « interface inactive » ne signifie pas « bus mémoire inactif ». Le deuxième cœur partage ce bus et ne remplace pas sa bande passante.

`Tab5DisplayBandwidth.cpp` augmente déjà la priorité de lecture AXI du maître mémoire DW-GDMA, sans modifier les priorités d'écriture. Ajouter une autre modification de QoS sans mesure ne corrigerait pas un réarmement DMA retardé par une section critique. La PSRAM est configurée en mode HEX à 200 MHz; le cache L2 occupe 256 Kio, avec lignes de 128 octets. La SRAM physique totale n'est donc pas équivalente au heap interne disponible.

## Pistes examinées sans défaut établi

- Le reset LCD du PI4IO est placé en entrée avec pull-up après l'initialisation. Les écritures ultérieures de la broche amplificateur ne sont donc pas une preuve de reset LCD. Aucune réinitialisation cyclique de la dalle n'a été trouvée dans `M5.update()`.
- `M5.update()` met notamment à jour le tactile sur Tab5. Il ne réinitialise pas périodiquement la dalle dans le chemin examiné.
- Aucun accès concurrent LVGL n'a été identifié dans cette analyse du pilote. Cache et présentation vérifient explicitement la tâche propriétaire.
- Aucun défaut de propriété d'un framebuffer n'a été démontré par cette lecture. Les barrières logiques actuelles ne suffisent toutefois pas à mesurer l'absence d'underrun matériel.
- Aucun parcours complet périodique du heap PSRAM n'a été identifié à l'accueil au repos par l'analyse complémentaire de la boucle UI. Il existe encore des chemins de télémétrie audio et d'admission sur actions. Le flash au repos doit donc rester une anomalie distinctement investiguée tant qu'une trace ne le corrèle pas à un événement.

## Corrections générales proposées et critères contradictoires

1. Sortir les parcours exhaustifs de heap des admissions normales. Conserver les compteurs rapides, les marges et les quotas; laisser l'allocation réelle établir la disponibilité d'un bloc contigu. Les diagnostics exhaustifs restent explicites ou attachés à un échec. Un essai doit prouver qu'une allocation fragmentée échoue proprement et ne produit pas une fausse réussite.
2. Pour les gros buffers dont l'OS possède la taille, remplacer les croissances par allocation séparée, copie hors verrou puis libération, avec conservation du bloc initial en cas d'échec. Ne pas présenter cette méthode comme sans coût : elle peut augmenter le pic lorsque l'ancien bloc aurait pu grandir sur place. Traiter les petits objets et l'allocation LVGL séparément afin de ne pas imposer une duplication permanente ou un échec injustifié.
3. Mesurer un compteur d'underruns avant/après une action, le plus grand intervalle entre complétions DSI et le temps des opérations mémoire suspectes. Une absence de progression du compteur **actif** pendant un flash réfute l'underrun comme explication de ce flash et réoriente vers les pixels publiés, la dalle et les commandes de puissance.

La séparation des tâches entre les deux cœurs peut réduire certains conflits de planification. Déplacer aveuglément l'initialisation des IRQ sur l'autre cœur ne corrige cependant ni les contentions PSRAM ni l'attente d'un verrou global de heap. Elle ne doit pas remplacer la suppression des opérations longues sous section critique.

## Protocole matériel minimal

Conserver le journal de démarrage, puis demander le diagnostic écran une fois avant et après chaque séquence : dix clics Dessin espacés; dix changements d'onglets Média; démarrage/arrêt MP3 puis FLAC; cinq minutes à l'accueil sans interaction. Relever séparément `underruns`, activité réelle de l'observateur, temps maximal entre complétions, nombre de soumissions et durée des opérations. Ne pas appeler un diagnostic exhaustif de mémoire au milieu du clic étudié : sa propre section critique pourrait perturber la mesure.

Un test hôte peut vérifier les appels, l'absence de parcours dans le chemin normal et les invariants de copie/libération. Il ne peut pas établir la latence d'une IRQ P4, l'absence de flash de la dalle ou l'absence de claquement audible.
