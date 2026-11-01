# Cartes : propriété des fichiers et intégrité des lieux — 0.8.45

Référence : livraison 0.8.44 conservée. L'audit a suivi le contrat réel `File` / `VFSFileImpl` du SDK M5Stack Arduino 3.3.9, au lieu d'assimiler un `File` faux à une absence de propriétaire. Aucun essai sur une Tab5 physique n'a été effectué.

## Pourquoi une reconnexion peut rester bloquée

`VFSImpl::open` crée un `VFSFileImpl` non nul même lorsque `fopen` a échoué. Un pointeur d'implémentation non nul n'établit donc pas que son fichier est ouvert. La couche VolumeFS de référence enveloppait cet objet invalide et lui conservait une référence de volume. Un membre `File` persistant peut ainsi être faux tout en retenant cette référence.

Cartes possédait précisément ce type de membre, `_offlineFile`. `closeOfflinePack()` appelait `close()` seulement si `operator bool` retournait vrai. Cela confondait lisibilité présente et propriété à libérer. Un fichier invalidé par une génération du support échouait également à ce test. La référence restante empêchait le contrôle `tab5SdOpenFiles()==0` d'autoriser le démontage/remontage, même après une demande manuelle de reconnexion.

La correction Cartes appelle inconditionnellement `_offlineFile.close()`, qui est idempotent dans le `File` réel du SDK et libère son implémentation même lorsqu'elle est fausse. Les buffers d'index sont toujours libérés ensuite et leurs états réinitialisés. Le correctif central VolumeFS, décrit dans l'audit Stockage, évite en outre d'envelopper une implémentation invalide à son ouverture.

Les essais Stockage et d'intégration utilisent le vrai `FS.cpp` et le vrai `vfs_api.cpp`, avec frontières POSIX adaptées : référence persistante de 1 à 0 ; fermeture Cartes après invalidation de 1 à 0 ; le service de récupération peut alors atteindre les appels de remontage au lieu de retourner indéfiniment. Ce résultat établit un blocage logiciel reproductible ; il ne prouve pas que toute perte physique de carte avait cette origine.

## Lieux enregistrés : ne pas confondre échec de lecture et liste vide

Un deuxième défaut pouvait provoquer une perte de données après une erreur SD : `loadPlaces()` effaçait `_placeCount` avant de lire le JSON. L'échec d'ouverture, de lecture ou d'allocation JSON laissait donc une liste vide. L'initialisation générale ne distinguait pas cet échec d'une collection disponible. Un ajout explicite ultérieur enregistrait uniquement le nouveau lieu et remplaçait le fichier précédent.

Le correctif sépare la disponibilité des catalogues et la validité de la collection de lieux :

- `_placesLoaded` n'est vrai qu'après lecture JSON et validation de la structure `places`, ou absence vérifiée du fichier principal et de sa sauvegarde.
- Les erreurs d'ouverture, de lecture, de fermeture, d'allocation JSON et de structure conservent la table précédente en mémoire. Aucun fichier n'est modifié par une lecture.
- Lorsque seul `.bak` existe, il est lu directement. Sa restauration n'impose plus de renommage pendant le chargement ; une publication reste associée à une sauvegarde explicitement demandée.
- Ajouter, renommer ou supprimer après un chargement échoué commence par réessayer ce chargement. Si l'erreur persiste, aucune publication n'est autorisée. Après sa résolution, l'action est appliquée à la collection récupérée, pas à une collection vide inventée.
- Les noms sont copiés de façon bornée en UTF-8 sans allocation supplémentaire après validation JSON. Cela évite qu'une allocation de `String` échouée au milieu de la phase de copie soit interprétée comme un nom vide à ignorer. La normalisation existante des espaces et la limite de 64 octets restent conservées.

Une carte neuve sans principal ni sauvegarde reste un cas normal : sa liste vide est valide et le premier lieu peut être créé. Aucune option d'autosauvegarde n'est ajoutée ou modifiée. Les noms de fichiers et le format JSON restent identiques.

## Essais exécutés

`tools/maps-places-tests-0845/run.py` exécute les méthodes réelles de chargement, sauvegarde, ajout, renommage, suppression et publication atomique, avec **ArduinoJson réel** et les **sources File/VFS réelles du SDK**. Les échecs sont injectés au niveau des appels POSIX (`fopen`, `fread`, `stat`, `fclose`) et de l'allocateur JSON. Les structures String/Stream, mutex, USB et la translation du chemin `/sd` sont des frontières hôte déclarées, pas une simulation du résultat d'ouverture des fichiers.

Dans les 35 contrôles de référence, chacun des trois enchaînements « erreur ouverture / lecture / allocation JSON → ajout après disparition de l'erreur » reproduit la disparition d'Alice et Bob du document enregistré. Les **280 contrôles courants** vérifient :

- conservation de la table et des octets du principal pendant l'erreur, sans ouverture en écriture ni renommage ;
- reprise de l'ajout après résolution, avec Alice, Bob et le nouveau lieu tous présents ;
- sauvegarde `.bak` seule, échecs lors de sa lecture, collection réellement absente, tableau vide valide et mauvais schémas ;
- noms UTF-8 tronqués proprement, renommage, suppression, échec de publication et support indisponible ;
- zéro référence SD et zéro allocation JSON suivie après les scénarios.

ASan et UBSan sont actifs. LSan est désactivé car non exploitable dans cet environnement. Les appels réels au SPI, FatFs, DMA, les temps du support et son alimentation ne sont pas exercés. Les compteurs sont des assertions hôte, pas des mesures de fluidité ou de mémoire libre sur la tablette.

La compilation ESP32-P4 de `Tab5Map.cpp` utilise la base de compilation 0.8.45 et ses en-têtes actuels. Les preuves et empreintes sont `tools/verification/maps-places-0845.json` et `tools/verification/maps-target-0845.json`. Les preuves de reconnexion réelle du service sont dans les rapports Stockage et d'intégration de cette version.

## Recherche Internet d'abord, repli local ensuite

La demande normale privilégie désormais Internet. La préférence est conservée dans la requête, et le worker consulte l'état effectif du réseau au moment de l'exécuter : une connexion arrivée pendant l'attente peut être utilisée sans ressaisie. Cette préférence n'allume jamais le Wi-Fi. Le dialogue de l'interface utilise une formulation générale et transmet explicitement la préférence en ligne.

Une correspondance Internet réussie est présentée sans parcourir l'index SD. Une erreur HTTP/TLS, un service injoignable ou une réponse sans résultat déclenchent ensuite la recherche locale, à condition que la demande soit toujours courante. Avant ce repli, les résultats partiels et le transport sont libérés. Les lieux enregistrés et points Mesh capturés avec la demande restent disponibles. Le code HTTP/TLS et le nombre de résultats sont journalisés techniquement avant le repli ; la requête et les identifiants réseau ne sont pas ajoutés à cette trace.

En l'absence de connexion, ou lorsque la radio possède le transport partagé, le service en ligne est indisponible : la recherche locale reste immédiatement utilisable. Le worker n'attend plus indéfiniment la fin d'un flux radio pour effectuer une recherche. Une demande remplacée ou une application fermée ne déclenche pas de nouveau parcours SD après l'annulation d'HTTP. L'intervalle existant entre recherches du fournisseur reste respecté.

`tools/maps-worker-tests-0845/run.py` exécute les méthodes réelles de requêtes, worker, lecteur de villes, générations et propriété des résultats, avec l'index livré effectivement lu. Les frontières HTTP, files FreeRTOS, mutex, allocations et fichiers sont des adaptateurs hôte déclarés. Les tests de propriété File/VFS réels restent distincts dans les rapports Stockage et lieux.

Les 140 assertions de référence établissent l'ancien ordre local par défaut avec connexion active, et l'absence de repli après une erreur HTTP 503. Les 655 assertions courantes couvrent : correspondance Internet sans lecture SD, erreur 503, résultat vide, déconnexion, absence de Wi-Fi, connexion apparue avant exécution, radio active, annulation HTTP par remplacement ou fermeture, repli avec lieux et Mesh.

## Zoom libre et générations de tuiles

Aucun verrouillage du zoom pendant le chargement n'est ajouté. Le changement de génération retire les requêtes et résultats encore dans les files ; un téléchargement en cours garde sa propre allocation jusqu'à sa fin/annulation. Le worker libère une tuile périmée qui ne peut plus être publiée. Une tuile déjà sortie de la file appartient au service UI : le changement de génération ne doit pas libérer ce pointeur.

Le cache est adressé par fournisseur et coordonnées de tuile (`style`, `z`, `x`, `y`), indépendamment du canevas et du zoom actuellement affichés. Une ancienne tuile téléchargée peut donc être enregistrée sans représenter la nouvelle vue ; le contrôle de génération du rendu reste nécessaire. Aucune corruption démontrée ici ne justifie de bloquer le zoom jusqu'à la dernière tuile.

Le test du worker change cinquante fois de génération pendant un téléchargement bloqué : le résultat périmé est libéré et seul le résultat courant arrive. Après transfert de sa propriété vers un résultat en attente dans l'UI, quarante autres générations ne modifient ni ne libèrent ses octets. Un résultat ancien encore dans la file est, lui, libéré une seule fois. Une recherche locale bloquée garde son identifiant distinct et peut terminer après des changements de zoom ; une requête de recherche remplacée libère son lecteur. Les compteurs de payloads, fichiers et baux reviennent à zéro en fin de sessions.

Ces essais n'exécutent ni le SPI ni la dalle. La liberté du geste/roulette dans l'interface LVGL et les accès physiques au cache doivent être distingués de ce contrôle de propriété des résultats.

## Ce que cet audit ne justifie pas

`storeTile` possède déjà une vérification de disponibilité avant l'écriture, et le passage réseau→cache est déjà borné par le résultat en attente du service UI. Les lectures hors ligne du worker possèdent un bail de stockage. Aucun défaut démontré ici ne justifie d'ajouter une file arbitraire, un délai supplémentaire ou un remontage concurrent avec des handles vivants.

Il faut encore tester sur l'appareil déplacements/zooms soutenus, lecture hors ligne, reconnexion depuis Réglages puis lecture/écriture d'un fichier neuf. Conserver les références ouvertes et le motif de refus de récupération. Les erreurs physiques `EIO`/`ETIMEDOUT`, une saturation de descripteurs et une carte pleine restent des causes distinctes ; la correction de propriété ne prétend pas réparer les contacts ou le bus.
