# Réseau et multimédia — audit 0.8.42

## État initial établi avant modification

Source : archive 0.8.41 fournie, SHA-256 `01389322d10ae80b668ce369b2586e41831af03e3e407ecd3d60da62dd2667ff`, recopiée en 0.8.42. Aucun essai sur Tab5 physique n’est disponible ici. Les mesures hôte ci-dessous caractérisent le code et ses invariants, pas la latence ni la SRAM réelle de la tablette.

| Anomalie confirmée par le code | Impact | Référence initiale |
|---|---|---|
| Chaque accès scalaire audio construit un snapshot contenant six String ; plusieurs appels par tour UI | Copies/allocations inutiles et temps sous mutex, y compris au repos | Tab5Audio.cpp, 16 accesseurs scalaires et 5 accesseurs texte |
| stop() vidéo attend la fin du worker jusqu’à 5 500 ms depuis les callbacks UI | Gel de LVGL pendant fermeture réseau/vidéo ; vTaskDelay ne traite pas les événements UI | Tab5Video.cpp, STOP_WAIT_MS et stop ; stopVideoViewer dans .ino |
| Catalogue audio/vidéo chargé depuis UI ; scan jusqu’à 4 096 entrées et tri sous verrou | Temps d’ouverture et interactions retardées ; le yield du scan ne restitue pas LVGL | ensureLibraryLoaded/reload et scanFolder |

Les copies de chaînes sont une cause de travail superflu confirmée ; leur part dans le lag ressenti reste à mesurer sur matériel. La fermeture synchrone explique un gel pendant les changements vidéo ; elle n’explique pas à elle seule le lag permanent. Aucun lien démontré ici entre réseau et flashs turquoise.

## Architecture et contraintes constatées

- Audio : worker FreeRTOS 16 Kio, priorité 5, autre cœur que l’UI ; producteur radio 16 Kio priorité 4 sur le cœur UI ; speaker M5Unified priorité 6 sur l’autre cœur. Les tâches audio/radio et les ressources I2S sont retirées quand inutilisées. Publication audio au plus toutes les 50 ms hors changement ; RSSI RPC sur worker au plus toutes les 5 s à l’arrêt.
- Vidéo : worker 24 Kio priorité 5 ; trois frames protégées par index UI, buffers du décodeur alloués par jpeg_alloc_decoder_mem (contraintes DMA/alignement conservées). L’image référencée par LVGL doit être détachée avant libération. Fin de worker publiée après fermeture socket/fichier, décodeur et libération de la priorité audio.
- Audio UI : commandes dans une file non bloquante, snapshots protégés par mutex. StateGuard récursif protège les autres états et catalogues ; il ne faut pas le retenir pendant des I/O longues accessibles à l’UI. La priorité audio existante diffère les nouvelles lectures pendant Held/Release puis restitue la dernière intention ; Stop l’annule.
- Réseau : Tab5NetworkGate sérialise les travaux réseau avec annulation coopérative. HTTP/TLS sont effectués dans les workers appelants. Les buffers TLS compatibles passent par l’allocateur externe ESP-IDF installé au démarrage ; aucune assimilation PSRAM/DMA interne.
- Stockage : Tab5StorageUse empêche un démontage concurrent, pas les mutations concurrentes sur un même chemin. Le transfert HTTP utilise un temporaire exclusif, vérifie écritures/fsync/close puis remplacement avec sauvegarde et restauration en cas d’échec. Pas de verrou global transactionnel ajouté sans scénario démontré.
- Compatibilité P4/C6 : la politique actuelle distingue Unknown/Same/Different à titre diagnostique ; une version inconnue ou différente ne bloque pas le démarrage Wi-Fi. L’échec d’inscription aux événements de diagnostic ne bloque pas le pilote. Pas de réintroduction de filtre de version.

## Corrections et validation

### Changements livrés

1. `Tab5Audio.cpp` : les 16 accesseurs scalaires lisent seulement le membre demandé du snapshot, avec le mutex et les valeurs d’échec existants ; les 5 accesseurs texte copient seulement leur texte. Le snapshot complet demeure disponible pour les écrans qui en ont besoin. Les comparateurs des pistes et radios utilisent `strcasecmp` sans créer de String temporaires ; ordre par catégorie puis nom et tri stable des radios conservés. Le SDK Arduino 3.3.9 applique précédemment `tolower` octet par octet puis `strcmp` ; aucune locale n’est activée par le firmware. Ce changement ne prétend pas ajouter un tri Unicode.
2. `Tab5Video.cpp/.h` : `stop()` demande l’arrêt et rend la main ; le worker garde ses ressources jusqu’à leur fermeture. Une seule intention de remplacement séparée de la source active conserve le dernier choix utilisateur. `service()` sur le thread UI la démarre après publication atomique de fin du worker. Une nouvelle fermeture annule l’intention. La vue montre Opening pendant l’attente et ne publie pas de frame provenant de l’ancienne source sous le nouveau titre. Aucun kill de tâche ni libération sur expiration d’un délai. La revue indépendante a également corrigé la fenêtre où LVGL se détachait après le cleanup de taskMain mais avant la publication finale : taskEntry revérifie l’attachement et libère les frames sous le même mutex, sinon elles restaient retenues et pouvaient empêcher la vidéo suivante, plus grande. Les échecs réels d’ouverture/mémoire restent Error.
3. `Tab5NetworkHttp.cpp` : les erreurs de métadonnées autres que ENOENT sont refusées sans renommer les fichiers existants. La version initiale confondait EIO avec absence à trois endroits (destination au départ, destination au commit, copie de récupération). Trois injections sur le code initial ont reproduit une perte de protection et sont maintenant préservées. Aucun formatage, migration, écriture périodique ou changement de politique d’enregistrement.

4. `Tab5Audio.cpp`, contrôle final du SDK : `File.flush()` ignore les retours de fflush/fsync, et `getWriteError()` seul ne détecte pas leurs EIO. L’enregistrement vérifie maintenant chacune des 13 écritures WAV, refuse un en-tête initial incomplet, capture errno immédiatement après chaque flush périodique/final et après close. Une finalisation échouée ne publie ni « sauvegardé » ni nouvelle piste ; le fichier partiel déjà écrit est conservé pour récupération. Le nettoyage d’abandon préexistant est conservé et son message ne masque plus l’erreur d’écriture de serviceRecording. Les choix d’enregistrement et la cadence des flush ne changent pas ; aucune sauvegarde automatique à l’incident ou à l’arrêt OS. Le flush vidéo est une opération PCM du pilote audio, pas un File.flush.

L’intégration UI ajoute `Tab5Video.service()` au loop. La séquence préexistante qui retire réellement la source LVGL avant `detachDisplay()` est conservée.

### Mesures exécutées

| Mesure | 0.8.41 | 0.8.42 | Portée |
|---|---:|---:|---|
| Copies String, 10 000 tours des 16 accesseurs scalaires | 960 000 | 0 | Méthodes C++ réelles, adaptateur hôte String ; pas une mesure de malloc P4 |
| Octets de texte copiés par ces accesseurs, jeu de données du test | 48 800 000 | 0 | Dépend des textes injectés ; pas un gain de SRAM permanente |
| Copies pour un appel à chacun des 5 accesseurs texte | 35 | 5 | Résultats inchangés, même protection |
| Copies String, 10 000 paires de comparaison piste + radio | 60 000 | 0 | 68 625 comparaisons de résultat, tous octets non NUL, préfixes et UTF-8 en locale C |
| Attente de fermeture vidéo dans le callback UI | Boucle jusqu’à 5 500 ms | Aucune boucle de join | La durée réelle de fermeture réseau n’est pas raccourcie artificiellement |
| Taille statique symbole `Tab5Video` | 208 octets | 272 octets | ELF P4 fourni et objet P4 recompilé ; +64 octets pour une intention bornée |
| Frame de pile compilée `wifiConnected()` | 160 octets | 16 octets | Prologue du ELF .41 et `.su` GCC P4 .42 ; pas le pic de pile cumulée |
| Scénarios EIO protégeant les fichiers HTTP | 0/3 | 3/3 | Fichiers POSIX réels, réseau et fautes simulés |

Aucune nouvelle tâche ou pile permanente. La pile vidéo reste 24 Kio ; ses plus grandes frames locales compilées sont 4 336 octets (AVI) et 4 224 octets (MJPEG brut), à ne pas confondre avec la pile cumulée. Une mesure matérielle du high-water mark reste nécessaire avant de les réduire.

### Vérifications exécutées

- Compilation ESP32-P4 avec le SDK M5Stack 3.3.9 restauré : `Tab5Audio.cpp`, `Tab5Video.cpp`, `Tab5NetworkHttp.cpp`, aucune erreur. Rapport `tools/verification/media-target-0842.json`. Cette compilation de lot ne remplace pas l’édition des liens complète du rapport global.
- ASan/UBSan, méthodes de production extraites : accesseurs audio (valeurs, mutex, avant initialisation et échec), vidéo (remplacement avant entrée worker, dernière intention, annulation, mémoire, revalidation SD, un seul propriétaire, absence de join UI, détachement imposé entre taskMain et publication finale, conservation d’un frame LVGL attaché et libération du frame détaché après terminaison).
- ASan/UBSan, enregistrement : 13 écritures d’en-tête courtes, flush périodique EIO avec writeError nul, 4 échecs de finalisation (flush, close, header, dernier bloc PCM), errno obsolète effacé, partiel conservé et ancien dernier fichier préservé, abandon inchangé.
- ASan/UBSan, comparateurs exacts ancien/nouveau : 68 625 comparaisons, ordre inchangé en locale C.
- ASan/UBSan, implémentation HTTP réelle avec réseau simulé et fichiers hôte : trois fautes EIO ; régressions de téléchargement tronqué, erreur écriture, annulation, délais, FD nettoyés, HTTPS sans horloge, refus downgrade TLS, redirections et sorties limitées.
- ASan/UBSan, adaptation C6 de production et méthodes Wi-Fi extraites : 45 862 vérifications, dont versions inconnues/différentes, événements concurrents et échec du monitoring ; absence de RPC de version dans les symboles du test. Aucun durcissement de version introduit.
- ASan/UBSan, NetworkGate réel : annulation, activation/désactivation, acquisition concurrente et propriété du mutex.

Les limites de chaque fixture sont dans son rapport JSON. LeakSanitizer est désactivé dans cet environnement ; ASan et UBSan restent activés. Les fixtures ne remplacent ni le scheduler FreeRTOS, ni LVGL, ni TLS/C6/SD/DMA physiques.

### Résiduels classés

- **Impact moyen, confirmé** : parcours catalogue audio/vidéo encore synchrone (limites de 4 096 entrées, 800 pistes/radios et 300 vidéos). Le passer naïvement au worker audio bloquerait le décodage et garderait l’UI en attente du verrou. Une évolution séparée devra construire un résultat hors verrou puis publier, avec identité stable de piste et politique pendant lecture. Le tri évite désormais le surcoût de copies, sans prétendre éliminer la latence SD.
- **Impact élevé si pilote bloqué, à confirmer matériel** : fermeture radio attend la sortie du producteur même après l’avertissement à 6 s, pour éviter un use-after-free. Elle s’exécute sur le worker audio, pas LVGL ; si le pilote ne sort jamais, la fonction audio reste indisponible. Capturer les traces radio/stack et l’état du transport en coupure pendant TLS ; aucun kill ni reboot ajouté pour masquer ce problème.
- **Impact à mesurer** : `radio_net` priorité 4 sur cœur UI, mais cède déjà réellement un tick après 4 ms de rafale, 2 ms sans données et 20 ms si le ring est plein. Pas de starvation continue démontrée ; priorités conservées. Son high-water mark est scanné à chaque lecture de 16 Kio : coût diagnostique à mesurer en forte rafale, absent au repos puisque la tâche est retirée.
- **Intégrité, limite existante** : le téléchargement protège son temporaire avec O_EXCL et conserve la récupération en cas de rollback raté, mais n’est pas une transaction universelle entre applications qui éditent le même chemin. Pas de garantie de résistance totale à une coupure au milieu des renames FAT ; ne pas l’annoncer comme telle.
- **Aucun défaut établi dans ce lot** : `hostedGetHostVersion` et `hostedGetSlaveVersion` du SDK 3.3.9 lisent seulement les structures en cache. Le diagnostic réseau UI ne déclenche donc pas la RPC de version soupçonnée ; conserver ce code évite de corriger un problème inexistant.


## À confirmer sur matériel

Mesurer les latences UI (p50/p95/max), CPU et tas interne/PSRAM avant/après dans les mêmes scénarios. Tester radio TLS, vidéo MJPEG/AVI, changement rapide de source, fermeture pendant DNS/handshake/réception, lecture audio après vidéo, retrait SD et reconnexion Wi-Fi. Les rapports hôte ne valident ni DMA/I2S réels ni protocole C6 ni planification FreeRTOS matérielle.

### Procédure matérielle courte

1. Après démarrage au repos, relever diagnostic tas interne/PSRAM (libre, minimum et plus gros bloc), latences UI et tâches. Répéter avec le même SD et Wi-Fi sur .41 puis .42 ; déplacer la souris pendant 30 s. Ne pas déduire le gain de latence des seuls compteurs de copies hôte.
2. Ouvrir un AVI local puis un MJPEG distant, fermer pendant connexion/réception, relancer rapidement trois vidéos puis fermer avant leur démarrage. L’interface doit répondre pendant la fermeture ; seul le dernier choix doit démarrer, jamais après Stop. Refaire vingt cycles et comparer les tas après stabilisation. Capturer `video-start`, `video-http`, `video-release`, `video-end` et `[STACK] video`.
3. Lire une vidéo PCM puis sélectionner audio/radio pendant sa fermeture ; vérifier réception de l’intention et reprise sans double sortie I2S. Désactiver Wi-Fi pendant vidéo/radio puis reprendre. Capturer messages `[radio]`, `[audio-cmd]`, `[NET]` et marges de piles.
4. Sur des fichiers de test uniquement, télécharger vers SD/USB avec interruption réseau, puis répéter avec un fichier existant sans `--overwrite`. Le fichier existant doit rester inchangé, aucune annonce de réussite sur échec. Ne pas retirer une carte contenant des documents ouverts pour provoquer une panne.
5. Ouvrir le catalogue sur une carte remplie : mesurer le délai restant et noter le nombre d’entrées. Le résiduel de scan synchrone est attendu ; il doit être traité séparément des gains des accesseurs.

6. Enregistrer un court son sur une carte de test disposant d’espace, arrêter normalement, relire le WAV. Répéter avec une carte de test pleine : vérifier l’erreur explicite et l’absence de faux « sauvegardé ». Lors d’un échec de finalisation demandé, conserver le fichier partiel pour diagnostic ; son en-tête peut être incomplet et il n’est pas annoncé comme valide. Ne jamais provoquer ce test sur des documents en cours.
