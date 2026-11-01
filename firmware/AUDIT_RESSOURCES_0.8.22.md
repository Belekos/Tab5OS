# Audit mémoire, tâches et ressources — 0.8.22

> Document historique, complété au fil des versions précédentes. La politique mémoire courante est décrite dans [MEMOIRE_0833.md](MEMOIRE_0833.md) : les purges périodiques et l’éviction du propriétaire visible/demandeur sont supprimées en 0.8.33.

L'audit a identifié et corrigé une fuite reproductible dans Pentest, une admission mémoire manquante au démarrage du partage, un contournement du contrôle réseau par la découverte des imprimantes, ainsi que des défauts de budget et de pile dans l'intégration Git. L'audit USB réalisé dans cette version corrige également des allocations partielles et la durée de vie des interfaces après déconnexion. Les workers ordinaires n'utilisent plus l'emplacement TLS réservé au SDK pour signaler leur fin.

**Le reset physique signalé à l’ouverture de Pentest n’a pas été reproduit sur tablette.** Les tests exécutent son contrôleur et son interface LVGL réelle sur ordinateur. Ils établissent les défauts décrits ci-dessous, sans prouver la cause de ce reset. Une trace RTC conserve maintenant l’étape Pentest en cours avant un redémarrage.

## Périmètre et preuves

Le périmètre comprend les modules natifs, le lancement ELF, la VM MicroPython, les services de fond, les allocateurs, les caches, les files, les propriétaires du réseau, du stockage et de l'audio, ainsi que les interfaces entre tâches et LVGL. L'inventaire reproductible `tools/pentest-tests/inventory.py` énumère les points de création des tâches et files dans les sources de l'OS et les pilotes HID/MSC : 271 fichiers source/en-tête et 35 sites de création dans la version figée. Son rapport contient les SHA-256 des sources examinées ; les moteurs Git/SSH possèdent en plus leurs inventaires et preuves propres. Les tâches internes d'Arduino, ESP-IDF, Wi-Fi, TCP/IP, mDNS et M5Unified sont distinguées des tâches de l'OS ; leurs consommations réelles dépendent du SDK et doivent être mesurées sur l'appareil.

| Contrôle exécuté | Résultat | Rapport dans `tools/verification/` |
|---|---|---|
| Contrôleur Pentest de production, 2 000 cycles ; ancienne suppression sans déroulement C++, nouveau dernier accès avant réclamation différée | .21 : 1 536 000 octets retenus ; .22 : zéro octet retenu ; 34 009 assertions pour .22 | `pentest-lifecycle-0822.json` |
| Interface Pentest de production avec LVGL 9.6 réelle, 40 cycles ordinaires et 4 rapports proches de 160 Kio | 505 assertions : ouverture, modes, texte entier, cadence, sélection, défilement, fermeture | `pentest-ui-0822.json` |
| Instantanés des tâches avec réponses FreeRTOS contrôlées | 4 118 assertions, 2 000 instantanés, aucune allocation transitoire retenue | `task-diagnostics-0822.json` |
| Démarrage du partage de production, 10 000 échecs injectés | 52 001 assertions, allocations libérées, aucun état actif laissé après refus | `share-admission-0822.json` |
| Découverte des imprimantes de production, 7 000 scénarios | 66 000 assertions : refus réseau, annulation, erreurs, libération des résultats et du propriétaire réseau | `printer-network-gate-0822.json` |
| Worker Terminal de production, 2 000 threads | 26 000 assertions : destructeurs exécutés, dernier accès publié, contexte libéré avant réclamation RTOS différée, aucune écriture TLS | `terminal-worker-0822.json` |
| Worker/service ELF de production, 1 024 scénarios | 22 528 assertions : dernier accès ordinaire, publication/suspension/suppression SDK des piles externes | `elf-lifecycle-0822.json` |
| Restitution du budget après libération/réduction, 2 000 cycles | 12 007 assertions ; bornage aux montants initiaux et refus d'un instantané admission incohérent | `quota-reservation-0822.json` |
| Vrai moteur PCRE2 embarqué et contrôleur regexp, 500 cycles | 14 173 assertions, 9 627 allocations, zéro octet retenu ; quota, OOM, annulation et limites | `git-regexp-quota-0822.json` |
| Initialisation/libération TLS Git, frontières mbedTLS simulées | 24 600 assertions, 1 600 cycles ; zéro allocation suivie retenue, liste complète des ciphers terminée | `git-tls-lifecycle-0822.json` |
| Initialisation globale réelle libgit2 hôte, 33 points d'échec répétés trois fois | 6 263 assertions, 90 initialisations refusées ; aucune allocation suivie retenue et réinitialisation saine | `git-init-oom-0822.json` |
| Clés locales libgit2 du profil sans threads, 1 000 cycles | 119 017 assertions : réutilisation des 16 emplacements, refus des indices hors tableau | `git-no-thread-tls-0822.json` |
| Listes de configuration Git, 432 sessions | 31 378 assertions, 84 échecs injectés ; zéro octet retenu et aucun accès au pointeur nul | `git-config-oom-0822.json` |
| Moteur Git final, 60 injections OOM et 200 commandes status répétées | 93 contrôles ; zéro allocation courante retenue, handles stables ; récupération d'orphelins explicitée ci-dessous | `git-0822.json` |
| Frames et chaînes d'appels directes des 216 objets Git embarqués P4 | 3 346 frames analysées ; limites de l'analyse explicites, aucune marge globale de pile affirmée | `git-stack-0822.json` |
| Compilation ciblée ESP32-P4 avec `-fstack-usage` | Unités mémoire, ressources, diagnostics, Pentest, outils, partage, réseau et imprimantes ; résultat et tailles de chaque frame dans le rapport | `memory-target-stack-0822.json` |
| Énumération des sources et créations de tâches/files | Couverture statique reproductible, pas une mesure de consommation | `resource-inventory-0822.json` |

Chaque preuve possède les hashes de ses sources et de son banc d’essai. Une modification invalide la preuve correspondante jusqu’à sa réexécution. Les frontières matérielles sont simulées dans les tests de cycle de vie. Aucun chiffre de SRAM, PSRAM libre ou marge de pile de la tablette n’est inventé. La compilation ciblée ne remplace pas la compilation complète ni l’essai matériel de la version.

## Défauts établis et corrections

### Pentest

Le worker .21 conservait des chaînes C++ locales puis appelait `vTaskDelete(nullptr)`. FreeRTOS ne déroule pas les destructeurs de la pile supprimée : ces chaînes restaient allouées. Le banc de régression reproduit ce comportement avec une vraie terminaison de thread sans déroulement C++, puis compte les allocations restantes. La charge est maintenant dans une fonction qui retourne avant la suppression de la tâche.

Le contexte Pentest est alloué explicitement en PSRAM. Il contient deux buffers fixes de 160 Kio : rapport du worker et instantané pour l’interface. Les cibles et paramètres ont des limites explicites. Le démarrage réserve la pile interne de 16 Kio, sa marge et le contexte manquant. Il refuse proprement un budget ou une allocation indisponible. La taille de pile n’a pas été augmentée sans preuve.

La fermeture demande l'annulation puis attend la publication du dernier accès au contexte. Cette publication intervient après le retour de la fonction d'analyse et la destruction de ses objets C++ ; ni le worker ni son trampoline ne touchent ensuite le contexte. Elle ne prétend pas que FreeRTOS a déjà libéré la pile et le TCB dynamiques : ceux-ci restent comptés dans le tas réel jusqu'au nettoyage du SDK. Un second démarrage ne peut réutiliser le contexte avant cette barrière. L'enregistrement d'un rapport terminé utilise directement son buffer stable ; il ne construit plus une copie `String` complète. L'interface prend un instantané au plus deux fois par seconde pendant l'analyse, conserve la sélection et la position de lecture, puis présente immédiatement le résultat terminé quand aucune sélection ne bloque le rafraîchissement.

La revue indépendante du contrôleur et des pointeurs UI n’a pas relevé de libération prématurée dans ces chemins. Les tests LVGL comprennent désormais le rapport presque plein. Ils n’établissent toujours pas l’origine du reset matériel initial.

### Partage et imprimantes

Le partage créait directement sa tâche de 8 Kio et contournait la réserve minimale de SRAM de l’OS. Il utilise maintenant le même mécanisme d’admission que les autres tâches natives. Les tests injectent les échecs du contexte, de la page HTML, de sa génération, de l’admission et de la création de tâche, puis vérifient le retour à un état arrêté et la libération des allocations.

La découverte des imprimantes appelait mDNS sans propriétaire réseau et sans respecter le commutateur réseau de l’OS. Elle prend maintenant un `Tab5NetworkLease`, avec attente d’admission bornée à une seconde, et vérifie l’état réseau avant les requêtes et après leur fin. Une requête mDNS déjà engagée reste bornée à trois secondes ; on ne détruit pas le service SDK sous une requête active. La désactivation empêche la requête suivante. Le propriétaire réseau est libéré sur sa tâche avant publication des résultats et suppression du worker. Le service mDNS commun du SDK reste géré par le SDK.

### USB

Voir `MANETTES_USB.md` et `gamepad-0822.json` pour les preuves propres au pilote. La version traite les allocations partielles et l’échec de démarrage, conserve le parent jusqu’au dernier acquittement des interfaces et empêche la perte d’une notification de détachement lorsque la file UI est pleine. Une revue croisée a aussi détecté puis fait retirer un marquage de déconnexion introduit par erreur dans suspend/resume. Ce chemin possède désormais ses propres tests. La compatibilité physique de chaque manette et les séquences rapides de débranchement restent à vérifier sur tablette.

### Fin des workers et emplacement TLS du SDK

ESP-IDF réserve l'emplacement TLS 0 à son implémentation pthread. Une tâche créée directement par FreeRTOS ne devient pas propriétaire de cet emplacement : une bibliothèque peut utiliser les fonctions pthread sans créer de pthread. Pentest, Terminal et les workers Git n'y enregistrent donc plus de callback OS. Ils publient leur dernier accès après libération de leurs objets locaux, puis terminent la tâche dynamique. Le contexte ordinaire peut être libéré à cette barrière ; une pile ou un TCB incorporé au contexte exigerait un protocole différent. Les tests Pentest et Terminal retardent volontairement la réclamation de la tâche après publication pour vérifier cette distinction.

La configuration P4 de libgit2 désactive ses threads. Aucun crash de pthread/libgit2 dû à ce conflit n'a donc été reproduit dans cette configuration ; c'est l'utilisation d'un emplacement réservé au SDK qui était incorrecte.

ELF distingue les deux cas. Avec une pile ordinaire, le worker publie son dernier accès après retour de l'entrée ELF et des appels audio/matériels. Avec une pile externe créée par `xTaskCreatePinnedToCoreWithCaps`, il annonce la fin puis se suspend ; l'interface vérifie réellement l'état suspendu et appelle `vTaskDeleteWithCaps` avant de nettoyer le contexte. L'annonce seule ou un état simplement bloqué ne suffisent pas. Aucun de ces chemins n'utilise l'emplacement TLS du SDK.

### Git : quota, initialisation et piles

Le moteur possède un plafond PSRAM de 8 Mio, en-têtes d'allocations compris. Les allocations libgit2, zlib et les contextes PCRE2 passent par ce quota. Un redimensionnement alloue le nouveau bloc avant de libérer l'ancien : le pic simultané des deux blocs doit tenir dans le plafond. Les buffers initiaux des compilations PCRE2 grandissent sur ce tas contrôlé au lieu d'occuper une frame initiale de 17 840 octets. La frame P4 correspondante est maintenant de 2 176 octets. Les buffers d'E/S Git de 64 Kio ont été ramenés à 2 Kio dans le profil embarqué ; `git_odb__hashfd` passe de 68 048 à 4 560 octets par frame.

Les expressions régulières sont bornées à 4 096 octets, 16 niveaux de parenthèses, une profondeur d'exécution de 128, 100 000 étapes de correspondance et 256 Kio de heap de correspondance. Les callbacks de PCRE2 permettent aussi d'observer l'annulation. Les tests exécutent le vrai moteur PCRE2 avec les petits buffers initiaux du profil embarqué, injectent les échecs de compilation et de correspondance, et vérifient la restitution des allocations.

La session réserve son budget avant l'initialisation globale de libgit2, qui initialise aussi mbedTLS pour des commandes locales. Elle ajoute au plafond Git 128 Kio de travail, 8 Kio internes et le budget TLS de l'OS. Ce budget TLS reste réservé pendant la session ; il n'est pas inclus dans le quota des allocations libgit2. Lorsque le hook global TLS PSRAM est actif, `tlsBudget()` prévoit 8 Kio internes et 256 Kio PSRAM ; son repli prévoit 48 Kio internes et 128 Kio PSRAM. Ce sont des admissions prudentes, pas des pics mesurés sur tablette. Le hook TLS est établi au démarrage de l'OS, jamais remplacé pendant une connexion concurrente.

Les échecs d'initialisation globale ont révélé des allocations non enregistrées pour cleanup. Les chemins système, filtres, pilotes de fusion et mbedTLS ont été corrigés. La liste des suites cryptographiques a désormais une case supplémentaire et un terminateur explicite lorsque toutes les suites sont reconnues. La session ferme ses objets et efface le callback d'annulation du port avant le shutdown global.

Le profil libgit2 sans threads avait également un compteur de clés locales qui ne réutilisait pas ses 16 emplacements après fermeture et acceptait une borne hors tableau. Les emplacements sont désormais marqués occupés/libres et les indices contrôlés strictement. Dans les listes de configuration, une allocation de tête n'était pas vérifiée et l'entrée pouvait être publiée avant une seconde allocation encore faillible. Les allocations requises précèdent maintenant la publication ; les échecs de copie/internement sont propagés et le parser restitue nom, valeur et entrée si l'ajout échoue. Les essais dédiés distinguent ces corrections des seuls compteurs de l'arène.

Un échec injecté plus tard pendant l'ouverture du dépôt conservait encore des blocs libgit2. Une arène bornée de session permet de récupérer ses blocs orphelins seulement après destruction des objets et retour du shutdown global à zéro. Les compteurs `reclaimedBlocks` et `reclaimedBytes` conservent cette information ; une opération normalement réussie qui requiert cette récupération est signalée en erreur. Cette récupération ne démontre pas à elle seule la fermeture de fichiers/sockets du SDK ni l'absence de pointeurs globaux périmés : les sweeps OOM et les redémarrages de session restent des preuves distinctes.

Dans le banc moteur final, 12 des 60 injections OOM utilisent ce filet de récupération : 17 blocs, 1 014 octets au total, au maximum 352 octets dans un cas. Tous les cas reviennent à zéro octet courant ; les trois commandes saines exécutées après chaque injection ne récupèrent aucun orphelin et le compteur de handles hôte reste stable à 117. Les 200 commandes `status` répétées passent également. Le résultat établit la récupération de ces cas, sans masquer l'existence des allocations orphelines des chemins OOM amont ni la distinction entre handles Windows et descripteurs ESP-IDF.

Git utilise un worker de 40 Kio, commun au profil de l'interface et au worker isolé du terminal. Ainsi les scripts et substitutions imbriqués du shell n'entament pas sa pile. L'analyse des objets effectivement embarqués donne un exemple de chaîne directe `git_clone` totalisant 19 088 octets, avec 27 cycles et 130 appels non résolus dans le graphe complet analysé. Ces sommes ne sont **pas** des maxima globaux : callbacks, récursions, SDK et chemins d'exception empêchent cette conclusion. La réserve de pile doit être observée sur l'appareil avec les scénarios réels.

## Inventaire des tâches de l’OS

Les tailles ci-dessous sont des **piles configurées en octets**, pas leur consommation mesurée. Les piles FreeRTOS ordinaires utilisent la mémoire interne ; le cas ELF avec pile externe déclarée est explicite. Les allocations du tas, les buffers DMA et les allocations internes du SDK s’ajoutent à ces piles. Une application visible peut utiliser plusieurs tâches et une application Python n’est pas un processus natif supplémentaire.

| Service / tâche | Pile configurée | Mémoire et propriétaires principaux | Fin, annulation et limites |
|---|---:|---|---|
| Boucle principale / interface | Fournie par Arduino | LVGL, modèles, écriture, lecteur, réglages ; caches UI en PSRAM | Tous les objets LVGL sont manipulés sur la tâche UI ; pas de suppression de worker depuis le collecteur |
| Réveil `tab5_alarm` | 6 144 | Propriétaire audio, petits buffers PCM | Arrêt coopératif, restitution audio et priorité mémoire avant suppression |
| Audio décodeur | 16 384 | File de 12 commandes ; catalogue borné ; décodage et périphérique audio | Tâche de service persistante ; libération des sources et décodeurs lors du changement ou de l’arrêt |
| Radio `radio_net` | 16 384 | Anneau PSRAM adaptatif de 64 Kio à 1 Mio, lecture de 16 Kio, session réseau/TLS | Fermeture coopérative et signal de sortie ; le propriétaire conserve son contexte si le SDK n’est pas encore sorti |
| ELF `tab5_elf` | 16 384 par défaut | Image validée ≤ 4 Mio ; allocations de l’ABI comptées, fichiers bornés ; console de 8 Kio | Les images demandant davantage de pile utilisent la variante PSRAM déclarée ; fermeture des fichiers et ressources de l’ABI après retour de l’application |
| Copie/déplacement `tab5-file-copy` | 12 288 | Énumération ≤ 1 024 entrées, profondeur 16, tampon PSRAM de 8 Kio, propriétaire stockage | Annulation entre blocs, fermeture des fichiers et transaction avant publication de la fin |
| Partage `sd-share` | 8 192 | Contexte et page HTML PSRAM, un client traité à la fois, backlog 2 | Délais des clients bornés ; propriétaire du mode réseau conservé tant que son arrêt n’a pas réussi |
| Firmware `tab5_firmware` | 12 288 | Fichier, hash, OTA ; buffer de 4 Kio sur pile | Fonction de travail avec portée C++ complète, fermeture et abandon OTA en cas d’erreur/annulation |
| JPEG `tab5_jpeg` | 16 384 | Encodeur et lignes PSRAM, dimensions ≤ 4 096 × 4 096 | Encodage par lignes, pas de canevas RGB complet ; fichier temporaire puis validation/renommage |
| Déverrouillage `vault_unlock` | 16 384 | Dérivation cryptographique, secrets bornés | Un job ; effacement du mot de passe et du résultat temporaire |
| Coffre `tab5_vault` | 16 384 | Enregistrement chiffré, contexte du coffre | Fonction de travail retourne avant suppression ; secrets et enregistrement temporaire effacés |
| Carte | 16 384 | Tuiles/cache PSRAM ; files : 32 demandes de tuiles, 1 résultat, 2 recherches, 1 résultat de recherche | Worker rendu inactif après fermeture ; annulation/restitution réseau ; cache protégé pendant impression/encodage |
| Pentest `tab5_pentest` | 16 384 | Deux buffers de 160 Kio PSRAM, réseau supervisé | Annulation entre opérations ; contexte libéré après dernier accès publié ; un seul travail actif |
| Impression / pages | 24 576 | Un job, raster borné à 4 Mio de pixels, lignes et polices PSRAM | Annulation coopérative ; instances de polices propres au worker, pas d’accès LVGL depuis celui-ci |
| Imprimantes `tab5_printers` | 8 192 | Jusqu’à 24 entrées ; nom ≤ 128 et URI ≤ 255 octets | Deux requêtes mDNS au maximum, chacune ≤ 3 s ; résultat remis à l’UI après libération du réseau |
| MicroPython `tab5_python` | 32 768 | Une VM, tas VM de 2 Mio PSRAM, source et console ; modèles/widgets bornés | Sortie coopérative ; VM, source, services et matériels libérés avant fin ; les apps Python partagent ce modèle |
| SDR `tab5_sdr` | 12 288 | État PSRAM, IQ 4 096, PCM 512 ; réserve interne USB additionnelle de 8 Kio | Libère flux USB et propriétaire audio ; spectre transmis à l’UI par état partagé borné |
| Terminal `tab5_shell` | 16 384 | Sortie circulaire et instantané de 32 Kio chacun ; historique et syntaxe bornés | Portées C++ terminées avant dernier accès publié ; arrêt coopératif ; contexte libéré à cette barrière |
| Git interface `tab5_git_ui` | 40 960 | Modèle PSRAM, 128 entrées par tranche et 16 widgets réutilisés ; moteur partagé avec terminal | Un job, commandes sérialisées ; annulation et fermeture du modèle après dernier accès publié |
| Git terminal `tab5_git_terminal` | 40 960 | Session de 8 Mio maximum plus travail et budget TLS ; arguments conservés par le shell en attente | Worker isolé des appels imbriqués du shell ; résultat acquis après dernier accès aux arguments |
| Vidéo | 24 576 | JPEG ≤ 2 Mio, image ≤ 1 280 × 720 × 2 octets, réseau et buffers PSRAM | Attente de fermeture bornée côté UI ; si le worker tarde, l’arrêt reste demandé et le contexte n’est pas réutilisé |
| USB `writer_usb` | 4 096 | Gestion bibliothèque USB ; files et objets pilotes | Service persistant ; cleanup d’installation partielle et acquittement des interfaces |
| HID `writer_hid` | 6 144 | 8 interfaces en PSRAM ; états touches/manettes par interface | Files : 24 rapports, 16 événements, 32 rapports souris ; détachements conservés hors file jusqu’à acquittement |
| Pilote HID | 4 096 | Contrôle et transferts USB, parent/interfaces | Parent libéré au dernier acquittement, y compris échec d’ouverture ou déconnexion pendant l’initialisation |
| Stockage USB / SDK | Selon pilote installé | File MSC de 8 événements, références aux fichiers ouverts, transferts | Le retrait/récupération exclusif attend la restitution des propriétaires ; pilotes tiers à mesurer sur appareil |

SSH est un moteur appelé par le terminal, pas une tâche permanente supplémentaire. L'allocateur de sa session est limité à 512 Kio PSRAM, avec admission séparée du budget TLS/crypto de l'OS. Le canal d'exécution utilise une fenêtre de 64 Kio et des paquets de 16 Kio ; les lectures/écritures sont traitées par petits buffers. Les allocations internes du moteur crypto ne sont pas assimilées au quota libssh2. Le banc natif final exécute mbedTLS 3.6.5 provenant du même sous-module officiel ESP-IDF 5.5.4 que la cible ; ses 149 contrôles SSH/SFTP et injections OOM passent aussi avec cette version. La configuration et les allocateurs Windows restent distincts du P4. Le pic natif de session de 186 567 octets n'est donc pas un pic de SRAM physique. La pile SSH sous scripts imbriqués doit être contrôlée avec les valeurs FreeRTOS, pas seulement une frame isolée.

## Politique mémoire et caches

Le gestionnaire préserve une réserve interne de 32 Kio et une réserve PSRAM de 512 Kio. Les budgets distinguent mémoire totale, plus grand bloc libre et bloc DMA. Les réservations décrivent les allocations futures : `allocated()` retire du budget les octets déjà matérialisés, déjà comptés dans le tas libre. Le trampoline n’autorise le worker à partir qu’après restitution de la réservation de sa pile, ce qui évite une double facturation transitoire.

Pour un quota réutilisable comme Git, `returned()` remet les octets dans la réservation **avant** leur libération dans le tas, sans dépasser les montants initiaux. Sans ce retour, plusieurs petits cycles pouvaient épuiser le budget prédit alors que le quota courant était redevenu nul. Un compteur de génération invalide aussi une admission dont la photographie mélangerait l'ancien budget réservé et le nouveau tas libre après restitution. Les tests couvrent ce croisement déterministe, les réductions, les libérations, le transfert de réservation et ses plafonds.

Le gestionnaire a au maximum 16 collecteurs de caches, 8 réservations et 8 demandes de collecte. L’attente d’admission est bornée ; une demande de récupération provenant d’un worker attend au plus 1 500 ms. Seule la tâche UI exécute les destructeurs des caches. Le contrôle périodique lit des compteurs toutes les 500 ms et temporise cinq secondes après une collecte sans gain. Il ne parcourt pas les heaps pour rechercher le plus grand bloc à chaque rafraîchissement. Le callback d’échec d’allocation ne fait qu’incrémenter un compteur atomique.

Les caches récupérables comprennent la mise en page de l’écriture masquée, la vidéo inactive, les catalogues audio inactifs, les listes de l’explorateur et leurs recherches, les listes audio et les tuiles de carte non utilisées par une impression. Le contenu utilisateur, les écritures en cours, les workers actifs et les fichiers ouverts ne sont pas traités comme des caches jetables. Le presse-papiers et l’historique d’édition sont des données conservées volontairement, avec limites explicites.

`Tab5ByteBuffer` et `Tab5PsramTable` utilisent explicitement la PSRAM sans repli interne ; un `realloc` refusé conserve le pointeur précédent. Les allocations génériques préfèrent la PSRAM après initialisation, mais le SDK peut encore choisir de la mémoire interne. Les piles, les structures du noyau, les transferts nécessitant du DMA et certaines allocations TLS/crypto doivent donc être observés séparément.

## Partage des périphériques et modèle de concurrence

`Tab5NetworkLease` sérialise les sessions gérées par l’OS. Un flux conserve son propriétaire jusqu’à fermeture et les clients doivent vérifier l’arrêt réseau. Les transitions de mode Wi-Fi sont réservées aux propriétaires OS. Ce mécanisme est un contrôle **coopératif** : il ne route pas les paquets à travers un VPN, ne met pas les applications dans un bac à sable et ne remplace pas un pare-feu. Les activités internes du SDK comme DHCP, Wi-Fi et mDNS ne deviennent pas un tunnel VPN par l’existence de ce verrou.

Les propriétaires stockage protègent les jobs contre un démontage pendant leur activité. Les récupérations et retraits sont exclusifs ; un fichier USB ouvert possède aussi sa référence au montage. Les transactions et fichiers temporaires limitent les écritures partiellement visibles, sans garantir contre toute coupure électrique du support.

L’audio possède un arbitre partagé pour lecture, réveil, SDR, Python et ELF. Les files et modèles UI sont bornés. Les workers de Pentest, audio, vidéo, Python/services, impression et copie n’appellent pas LVGL directement ; ils publient des résultats que la tâche UI présente ensuite. Le nouvel inventaire automatise également ce contrôle lexical, qui complète la revue des chemins sans constituer une preuve formelle de sûreté de tous les callbacks.

## Diagnostics utilisables sur la tablette

Dans le terminal, `memory` montre les tas libres et leurs blocs disponibles. `tasks` effectue un instantané réel FreeRTOS et expose identifiant, nom connu, état, priorité, minimum de pile restant en octets et compteur d’exécution. Ce relevé coûte un parcours des piles : il est déclenché explicitement, jamais à chaque frame UI. Les états numériques suivent FreeRTOS : 0 en cours, 1 prête, 2 bloquée, 3 suspendue, 4 supprimée, 5 invalide. Les compteurs d’exécution sont des unités du SDK, pas un pourcentage CPU.

L’instantané est alloué en PSRAM et limité à 128 tâches. Le registre des noms est limité à 32 entrées copiées : une tâche SDK ou une ancienne entrée évincée porte le nom `sdk`, mais garde son identifiant et ses compteurs réels. Le code ne déréférence jamais les noms, handles ou bases de pile des TCB après reprise de l’ordonnanceur ; ces pointeurs peuvent déjà avoir été libérés. Un manque de mémoire ou une croissance de la liste pendant la capture renvoie un échec propre.

Pour analyser un reset : ouvrir les diagnostics système dans les réglages et relever « Dernier reset » et « Avant le reset (contexte) ». Le journal série à 115 200 bauds présente également le type du reset et `[RESET]`. La trace Pentest indique `interface`, `interface prete`, `demarrage`, le nom de la commande ou `termine`, sans enregistrer la cible ni un secret. Elle décrit une dernière étape, **pas automatiquement la cause** du reset. La RTC n’est pas un journal persistant garanti après coupure d’alimentation.

## Limites et validation matérielle restante

1. **Reset Pentest : cause non établie.** L’ouverture vide et le rapport presque plein passent sur LVGL hôte. Un défaut de pilote, un état concurrent, un manque de SRAM ou un dépassement de pile dans les appels SDK restent à discriminer avec le type de reset, la trace RTC et la backtrace réelle.
2. **L’épuisement mémoire n’est pas récupérable partout.** Les flags effectivement utilisés par la compilation P4 conservent `-fexceptions`. L’allocateur de l’écriture lève donc `std::bad_alloc` en cas d’échec ; les appels qui ne capturent pas cette exception ne constituent pas une récupération contrôlée. Sa branche `abort()` ne s’applique qu’à une compilation sans exceptions. LVGL 9.6 possède également des chemins tels que `lv_spangroup_add_span` qui exécutent `LV_ASSERT_MALLOC` puis utilisent le pointeur sans retour d’erreur récupérable. Les contrôles des allocations OS réduisent le risque, mais ne rendent pas tous les chemins LVGL, libc et SDK récupérables sous épuisement total.
3. **Isolation coopérative.** Un ELF est du code natif de confiance dans le même espace mémoire. Un pointeur erroné ou une app qui ne coopère plus peut affecter l’OS. On ne peut promettre l’isolation de processus d’un système avec MMU. La suppression forcée d’un worker possédant un mutex ou un fichier n’est pas une réparation sûre.
4. **Piles des bibliothèques.** `-fstack-usage` donne les frames individuelles des unités compilées. Le maximum d’une seule fonction ne couvre ni les appels imbriqués, ni la récursion/substitution du shell, ni le SDK précompilé. Les piles sont à confirmer avec les charges réelles et les valeurs minimales FreeRTOS.
5. **Consommation et fragmentation physiques non mesurées.** Exécuter successivement ouverture/fermeture des apps, navigation/zoom carte, lecture locale et réseau, import, impression, Python, ELF, Pentest, SSH/Git et débranchements USB. Relever `memory` et `tasks` avant puis après stabilisation, et conserver le plus petit bloc interne/DMA ainsi que les minimums de pile. Un cache volontaire ne doit pas être confondu avec une fuite ; un plateau qui diminue à chaque cycle doit être investigué.
6. **Arrêt réseau et périphériques.** Mesurer les délais avec Wi-Fi instable, TLS interrompu, support retiré, files USB saturées et annulation pendant une opération de stockage. Les tests hôte injectent ces états aux frontières contrôlées, sans reproduire leur chronologie matérielle.

La version apporte des corrections vérifiées et des moyens de diagnostic supplémentaires. Cet audit n’est ni une certification de sûreté ni une affirmation que tous les scénarios matériels ont été testés.
