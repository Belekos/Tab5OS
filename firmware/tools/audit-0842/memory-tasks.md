# Audit mémoire et tâches — Tab5OS 0.8.42

## Référence, portée et preuves

Audit des sources réelles 0.8.41 reçues, reprises dans le projet 0.8.42. L'archive de référence `Tab5OS_0.8.41_Corrections.zip` a pour SHA-256 `01389322d10ae80b668ce369b2586e41831af03e3e407ecd3d60da62dd2667ff`. Le gestionnaire 0.8.41 exact est conservé dans `tools/memory-tests-0842/baseline-0841` pour comparaison exécutable. Aucun résultat sur Tab5 physique n'est revendiqué.

Périmètre corrigé : `Tab5ResourceManager.cpp/.h`, `Tab5Memory.cpp/.h`. Les politiques, marges, tailles de piles et capacités matérielles restent identiques. L'audit des autres modules est partagé avec les rapports affichage, applications, stockage et communications ; les ressources système compressées relèvent du lot compilation/Flash.

Preuves reproductibles : `tools/verification/memory-0842.json` (sources et tests hachés, commandes, résultats), `tools/verification/memory-target-0842.json` (compilation P4 exacte et cadres GCC). Le SDK réellement utilisé est M5Stack 3.3.9 / ESP-IDF 5.5.4, GCC RISC-V 14.2.0 esp-14.2.0_20260121, cible ESP32-P4.

## Diagnostic avant modification

| Constat | Certitude | Impact |
|---|---|---|
| Chaque tentative d'admission récupérable interrogeait les plus grands blocs interne, DMA interne et PSRAM, même lorsque les octets libres interdisaient déjà le lancement | Confirmé par le code 0.8.41 et comparaison hôte | Travail répété sous verrous des allocateurs pendant une pénurie ; potentielle perturbation des interruptions et de l'interface |
| Une admission peut réessayer chaque tick pendant 60 ms depuis l'interface, jusqu'à 1 500 ms depuis un worker | Confirmé, délais existants conservés | L'ancien coût des parcours était multiplié ; ces délais ne sont pas ajoutés par cette correction |
| `tab5ResourcesService()` ne parcourt pas les tas et ne purge pas de cache au repos sans demande | Confirmé | Ce module seul n'explique pas le lag continu de la souris au repos |
| Le worker cartes p2 et le réseau radio p4 partagent le cœur UI p1 dans le montage réel | Confirmé | Contention possible pendant ces activités ; influence réelle à mesurer, pas preuve d'une cause universelle |
| Les manques de mémoire interne peuvent coexister avec beaucoup de PSRAM disponible | Confirmé par capacités et tailles demandées | Piles usuelles, pilotes, DMA et blocs contigus ne sont pas remplaçables arbitrairement par PSRAM |
| Les tas/piles réels sont trop petits, fragmentés ou fuyants dans un scénario utilisateur donné | Non mesuré sur appareil | Nécessite instantanés et cycles décrits ci-dessous ; aucune réduction aveugle de pile |

## Ce que signifient les mémoires affichées

Les 768 Kio du P4 désignent la SRAM HP physique, pas un tas libre de 768 Kio. La configuration du SDK affecte `CONFIG_CACHE_L2_CACHE_SIZE=0x40000`, soit 256 Kio, au cache L2. Le code/data internes fixes et les réservations consomment une autre partie. Le reste seulement devient tas enregistré. Un total de tas de quelques centaines de Kio n'est donc pas contradictoire avec 768 Kio physiques. La valeur exacte de l'appareil n'a pas été mesurée ici.

| Région/capacité | Comptage correct utilisé | Contraintes et statut |
|---|---|---|
| HP SRAM physique | 768 Kio ; cache L2 256 Kio avec ce SDK | Code, données, cache et tas partagent cette enveloppe |
| HP SRAM enregistrée au tas | `INTERNAL | DMA | 8BIT` pour cette implantation P4 | Les deux types HP du port mémoire IDF portent ces capacités ; ne pas soustraire des plus grands blocs pour inventer une partition |
| SPM / TCM | 8 Kio physiques ; `TCM | 8BIT` au tas | Deux noms de la même banque, ne pas les additionner |
| LP SRAM | 32 Kio physiques ; `RTCRAM | 8BIT` au tas | Une partie peut être réservée et ne pas appartenir au tas, notamment diagnostic retenu |
| Interne agrégé | `INTERNAL | 8BIT` | Recouvre HP + TCM + LP enregistrées ; ne pas l'additionner aux lignes détaillées |
| PSRAM | `SPIRAM | 8BIT` ; matériel 32 Mio | Accès avec cache, latence et contraintes DMA/alignement propres ; pas une extension interchangeable des piles/pilotes internes |
| DMA | Une capacité, pas une mémoire physique supplémentaire | Selon périphérique, alignement, descripteurs et cohérence cache nécessaires ; une allocation SPI RAM ordinaire n'en garantit pas l'usage |
| IRAM/DRAM | Affectations de code/données internes et capacités d'accès | Ne pas les présenter comme des banques indépendantes ajoutées aux 768 Kio |

`heap_caps_get_minimum_free_size()` somme les minima des régions correspondant aux capacités : ces minima ne sont pas nécessairement contemporains. C'est un indicateur conservateur de pression, pas une mesure simultanée de consommation maximale. `largest_block` décrit une allocation contiguë possible ; `free` seul ne suffit pas à prédire un succès.

Le SDK propose initialement `SPIRAM_MALLOC_ALWAYSINTERNAL=4096`, mais `tab5ResourcesBegin()` remplace volontairement ce seuil par `heap_caps_malloc_extmem_enable(0)`. Les `malloc/new` ordinaires privilégient donc déjà la PSRAM. Les tâches et pilotes qui demandent explicitement l'interne continuent à l'utiliser. Cette politique existante n'est pas changée. `CONFIG_FREERTOS_TASK_CREATE_ALLOW_EXT_MEM=1` autorise techniquement les piles externes explicitement demandées ; cela ne valide pas toutes les bibliothèques ni tous les chemins cache désactivé. La pile ELF externe demeure une capacité explicitement déclarée, pas une isolation mémoire.

## Architecture du gestionnaire et propriété

Le gestionnaire possède huit réservations de lancement, huit demandes de collecte et seize entrées de caches. Une réservation compte les allocations futures ; `allocated()` retire les octets matérialisés du quota restant. `returned()` restaure le quota avant libération et modifie un compteur de génération pour invalider un échantillon concurrent. Les marges de 32 Kio internes et 512 Kio PSRAM sont conservées.

Les tâches auxiliaires demandent la collecte au thread UI. Celui-ci seul exécute les callbacks de cache ; les caches du premier plan et du demandeur sont protégés. Le collecteur distingue le domaine interne de la PSRAM et ne détruit que des caches annoncés rechargeables. Pas de purge périodique, de sauvegarde forcée ou de tâche supplémentaire. Le verrou d'admission existant est borné à huit tentatives avec possibilité de laisser l'autre tâche progresser ; il ne constitue pas un ordonnanceur alternatif.

`Tab5Task.h` conserve la passation du budget au wrapper de tâche : le worker ne commence pas son travail tant que la création n'a pas publié la propriété de la réservation. Les tâches admises Python/cartes/vidéo ne paient pas une deuxième fois leur pile. Les callbacks de fermeture et les échanges asynchrones applicatifs sont traités dans le lot cycle de vie des applications.

## Inventaire des tâches

Valeurs relevées dans les appels du projet et la configuration SDK, en octets, avant réduction éventuelle : **aucune pile n'a été réduite par ce lot**. Les piles usuelles sont internes. Core `UI` vaut 1 avec `CONFIG_ARDUINO_RUNNING_CORE=1`. Une tâche à la demande n'est pas nécessairement présente au repos. Les TCB, mutex, files, bibliothèques et piles SDK s'ajoutent aux tailles listées.

| Tâche / source | Pile | Priorité | Cœur | Durée / rôle |
|---|---:|---:|---|---|
| `loopTask` / `.ino`, Arduino | 24 576 | 1 | UI = 1 | Permanente, LVGL et services UI ; `SET_LOOP_TASK_STACK_SIZE(24*1024)` |
| `writer_usb` / WriterUsb | 4 096 | 3 | Libre | Permanente si USB installé, événements hôte |
| `writer_hid` / WriterUsb | 6 144 | 3 | Libre | Permanente si USB installé, événements clients |
| HID background / WriterUsb + pilote HID | 4 096 | 4 | 0 | Permanente si pilote installé |
| MSC background / Tab5UsbStorage + pilote MSC | 4 096 | 3 | 0 | Permanente si pilote installé |
| `tab5_usb_printer` / Tab5UsbPrinter | 4 096 | 3 | 0 | Service USB installé |
| `tab5_serial` / Tab5Serial | 4 096 | 2 | 0 | Service série activé |
| `tab5_map_net` / Tab5Map | 16 384 | 2 | UI = 1 | Worker cartes ; files et réponses séparées de LVGL |
| `tab5_audio` / Tab5Audio | 16 384 | 5 | 0 | Décodeur ; cycle de vie et file de commandes |
| `radio_net` / Tab5Audio | 16 384 | 4 | UI = 1 | Flux réseau, actif pour radio ; stream buffer |
| Speaker M5Unified | Selon bibliothèque | 6 configurée | 0 | Service I²S pendant audio ; vérifier avec instantané tâches |
| `tab5_video` / Tab5Video | 24 576 | 5 | 0 par défaut | Décodage vidéo |
| `tab5_python` / Tab5Python | 32 768 | 1 | 0 | Script ; VM 2 Mio en PSRAM séparée |
| `tab5_elf` / Tab5Executable | 16 384 interne, ou valeur déclarée externe | 2 | 0 | ELF ; en 0.8.42 le chargement utilise aussi le worker interne existant |
| `tab5_git_terminal`, `tab5_git_ui` / Tab5Git, UI | 40 960 | 1 | 0 | Opération Git, plus pile appelante terminal le cas échéant |
| `tab5_print`, `tab5_pages` / Tab5Print | 24 576 | 1 | 0 | Impression ou calcul de pages |
| `tab5_jpeg` / Tab5Jpeg | 16 384 | 1 | 0 | Encodage JPEG |
| `tab5-file-copy` / Tab5FileOperation | 12 288 | 1 | 0 | Opération fichiers et bail stockage |
| `tab5_shell` / Tab5Terminal | 16 384 | 1 | 0 | Commande en cours |
| `tab5_firmware` / Tab5Firmware | 12 288 | 1 | 0 | Vérification / installation demandée |
| `tab5_printers` / Tab5PrinterDiscovery | 8 192 | 1 | 0 | Découverte réseau |
| `vault_device` / Tab5KeyStore | 16 384 | 1 | 0 | Déverrouillage/appareil ou changement PIN |
| `vault_unlock` / Tab5KeyStore | 16 384 | 0 | 0 | Déverrouillage par mot de passe |
| `tab5_vault` / Tab5Vault | 16 384 | 0 | 0 | Chiffrement fichiers demandé |
| `tab5_alarm` / Tab5Alarm | 6 144 | 3 | 0 | Sonnerie |
| `tab5_meshtastic` / Tab5Meshtastic | 8 192 | 2 | 0 | Service mesh activé |
| `tab5_mesh_ble` / Tab5Meshtastic | 5 120 | 5 | 0 | Hôte NimBLE, valeur SDK |
| `mesh_store` / Tab5Meshtastic | 4 096 | 1 | 0 | Persistance d'historique selon options existantes |
| `sd-share` / Tab5FileShare | 8 192 | 1 | 0 | Partage réseau activé |
| `tab5_usb_format` / Tab5UsbStorage | 8 192 | 1 | 0 | Formatage explicitement demandé |
| `tab5_sdr` / Tab5Sdr | 12 288 | 1 | 0 | Réception SDR activée |

Les cinq piles USB listées représentent déjà **22 Kio internes**, même sans périphérique utilisateur actif une fois les pilotes installés. Avec loop (24 Kio) et le service série (4 Kio), ces seules piles peuvent représenter 50 Kio, sans les SDK/TCB/files. Ce n'est pas une mesure du total au démarrage.

SDK configuré : deux idle tasks de 1 024 octets, pile ISR 2 096 octets par cœur, timer FreeRTOS 2 048 octets priorité 1, `esp_timer` 8 192 octets, TCP/IP 4 096 octets, tâche de démarrage IDF 4 096 octets. Présence et durée dépendent du SDK et des services ; utiliser `tasks` pour l'état réel. Les tâches C6/ESP-hosted et les pilotes peuvent en ajouter. L'inventaire de code ne remplace pas cet instantané.

## Buffers et empreinte permanente

Déjà en PSRAM : VM Python 2 Mio à la demande, sources/console Python, données TTF (limite 6 Mio chacune), documents et aperçu, pixels/tuiles/cache et structures de cartes, workspace MP3/AAC, anneau radio 64 Kio à 1 Mio selon budget, buffers d'enregistrement audio, espaces de copie et de recherche. Le buffer LVGL déclaré dans setup est `SCREEN_W * 32 * 2` octets en PSRAM ; le framebuffer/pilote DSI est audité dans le lot affichage. Un tampon de travail PSRAM n'est pas automatiquement compatible avec la DMA d'un périphérique.

Registre existant des noms de tâches : 32 entrées PSRAM, 36 octets chacune sur P4 (1 152 octets), inscrit au début des tâches, pas à chaque tour UI. Le dump complet alloue temporairement une table PSRAM bornée à 128 `TaskStatus_t`, puis la libère. Les noms SDK inconnus restent `sdk` avec identifiant numérique ; une entrée ancienne peut être remplacée dans ce registre circulaire. Aucun pointeur TCB/nom/pile issu du snapshot n'est déréférencé après reprise de l'ordonnanceur.

Le nouveau callback de sélection des capacités ajoute un pointeur de 4 octets au gestionnaire global P4, désormais 820 octets (`nm -S` de l'objet cible). Le diagnostic mémoire n'ajoute ni tableau permanent, ni allocation dynamique, ni tâche. Son cadre local GCC est 432 octets ; cette valeur ne mesure pas la profondeur cumulée d'une pile.

## Corrections effectuées

1. **Admission** : une tentative de récupération commence par les compteurs d'octets libres. Si le budget et les marges ne tiennent déjà pas, elle refuse provisoirement sans parcourir les blocs. Dès qu'ils tiennent, toutes les contraintes contiguës demandées sont vérifiées. Le refus final garde l'échantillon complet, le motif, les quotas et le compteur de refus exacts.
2. **Capacités** : les tentatives de récupération et le choix du domaine à collecter ne parcourent que les tas dont un bloc est réellement demandé. Un lancement avec seule pile interne ne parcourt plus la PSRAM et le domaine DMA sans nécessité. L'ancien constructeur de Manager à deux callbacks reste compatible pour les tests/outils.
3. **Diagnostic explicite** : `tab5DumpMemory(emit, context)` émet total/libre/minimum/plus grand bloc par catégorie et l'enveloppe physique/cache/réservations. Il ne fait rien si l'émetteur est nul. La commande explicite est intégrée par le lot principal ; aucun abonnement périodique ne l'appelle.

Le diagnostic de tâches déjà présent est conservé. `CONFIG_FREERTOS_USE_TRACE_FACILITY` et les statistiques runtime sont actifs dans le SDK exact. Les high-water marks IDF sont en octets. `uxTaskGetSystemState()` suspend l'ordonnancement pendant la capture et le calcul des minima de piles ; il reste réservé à une commande volontaire, jamais une collecte permanente. Les compteurs runtime sont cumulés, pas des pourcentages CPU instantanés.

## Mesures et validation réellement exécutées

| Vérification | Avant | Après / résultat |
|---|---:|---:|
| Parcours de tas pendant pénurie interne stable, fenêtre hôte 20 ms, même scénario | 66 | 4 |
| Vérifications admission ciblées | 20 passent | 23 passent |
| Suite propriété/concurrence historique | — | 2 033 vérifications, 400 cycles passent |
| Vrai sampler/formatter compilé avec stubs SDK | — | 42 vérifications passent, huit combinaisons de capacités |
| Compilation de Tab5Memory.cpp et Tab5ResourceManager.cpp avec SDK P4 exact | — | Réussie sans avertissement |
| SRAM/PSRAM au repos, latence souris, CPU, fragmentation physique | Non mesurés | À mesurer sur Tab5 |

Les 66/4 sont des appels comptés sur modèle de tas hôte : le nombre d'essais baseline varie avec l'ordonnanceur hôte. Ce résultat établit la suppression du travail inutile, pas une réduction en millisecondes sur la tablette. Les quatre parcours restants sont la sélection du domaine interne à collecter (un), puis le refus final complet (trois).

Tests avec AddressSanitizer et UndefinedBehaviorSanitizer actifs. LeakSanitizer ne peut pas fonctionner dans l'environnement (accès ptrace/proc aux tâches interdit) : pas de succès LSan revendiqué. Les cycles vérifient la restitution des réservations et les chemins concurrents, pas l'absence de fuite d'un firmware exécuté sur appareil.

Reproduction depuis la racine : `python3 tools/memory-tests-0842/run.py --build /chemin/temporaire`. Le rapport conserve les commandes exactes et les empreintes des fichiers. Le rapport cible conserve les cadres GCC individuels : 432 octets pour dump mémoire, 304 pour dump tâches, 624 pour le wrapper de réservation avec diagnostic. Aucune de ces valeurs n'autorise seule une réduction de pile.

## Essais matériels requis et problèmes résiduels

1. Démarrage froid avec même SD/options, attendre l'accueil, relever mémoire détaillée et `tasks` une fois. Ne pas laisser les diagnostics répétés tourner pendant les mesures de fluidité.
2. Ouvrir/fermer 20 fois chacun : cartes hors ligne, dessin, Python court, ELF connu, puis radio/vidéo. Relever après le premier cycle puis après 20 cycles et une période idle permettant la récupération FreeRTOS. Comparer libre et plus grand bloc par capacité ; le minimum depuis boot descend par définition et ne remonte pas après fermeture.
3. Lors d'un refus Bluetooth/Python/Git, conserver le motif exact, budget, quotas en attente et plus grand bloc. Beaucoup de PSRAM avec petit bloc interne indique un besoin interne/fragmentation, pas automatiquement une erreur d'affichage. Demander `tasks` pour identifier les piles encore actives et leurs marges.
4. Pour la souris, comparer accueil sans activité, cartes, radio et copie. Utiliser le profilage UI optionnel du lot principal afin de séparer attente entre tours et travail actif. Désactiver ensuite ce profilage. Aucune preuve n'attribue ici les flashs DSI à une région mémoire.

Résidu de criticité haute : manque interne/fragmentation et high-water réellement observés non disponibles ; ne pas prétendre corriger toute pénurie Bluetooth par l'optimisation d'admission. Criticité moyenne : concurrence radio/cartes avec le cœur UI, à corréler avec les timings. Criticité moyenne : `tasks` lui-même perturbe brièvement l'ordonnancement, usage explicite seulement. Les garanties d'isolation d'une application ELF restent limitées par l'absence de protection mémoire propre à chaque application.
