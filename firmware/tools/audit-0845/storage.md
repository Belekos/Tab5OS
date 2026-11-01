# Stockage : propriété des fichiers et récupération — 0.8.45

Référence : livraison 0.8.44 immuable, SHA-256 `b5fdc983e51e17568d700b9ae1359e902fa61f43c86505a775e650f10cab16aa`. L'audit a lu l'implémentation File/VFS réellement installée de M5Stack Arduino 3.3.9, puis exécuté ces sources exactes sur hôte. Aucun support utilisateur ni périphérique physique n'a été modifié.

## Cause de blocage établie

Le contrat réel d'Arduino n'est pas « un échec d'ouverture renvoie un pointeur nul ». `VFSImpl::open` renvoie un `shared_ptr<VFSFileImpl>` non nul, même lorsque `fopen` ou `opendir` a échoué. Le fichier est alors **faux selon son opérateur bool**, mais l'objet existe et garde son chemin alloué.

`Tab5VolumeFS::Routes::open` ne testait que le pointeur. Il enveloppait donc cet objet invalide et transférait une référence SD au wrapper. Si une application conservait ce `File` dans un champ, puis faisait `if (file) file.close()`, elle ne le fermait jamais : le fichier était faux, mais la référence volume restait présente. La récupération disposait bien du bail exclusif, puis refusait de démonter à `tab5SdOpenFiles()!=0`. Demander de nouveau la reconnexion ne pouvait pas supprimer ce champ ; le redémarrage supprimait enfin les objets. Le test reproduit exactement ce blocage avec les véritables méthodes SDK.

La correction centrale `openedFile` vérifie aussi `bool(*impl)` avant d'envelopper un résultat d'ouverture ou d'énumération. Un objet représentant une ouverture échouée n'obtient plus de référence au volume ; sa destruction conserve l'erreur. Les descripteurs réellement ouverts restent comptés et continuent d'interdire un démontage dangereux. Les compteurs ne sont pas remis arbitrairement à zéro.

L'agent Cartes a aussi rendu `closeOfflinePack` inconditionnel : `File::close` est idempotent et doit libérer l'objet possédé même si sa génération de volume est devenue invalide. Cette défense respecte les vraies règles Arduino : une destruction de copie conserve les autres propriétaires, mais un `File::close()` explicite ferme également les alias du même implémentateur. Le commentaire de VolumeFS a été précisé en conséquence.

La présence de ce chemin démontre un blocage logiciel possible de reconnexion ; elle ne prouve pas à elle seule l'origine de chaque défaillance électrique/SPI ou de chaque disparition constatée sur votre tablette.

## Erreurs SDK et intégrité des données

Les branches VFS d'allocation refusée ne définissaient pas toutes `errno`. Des fallbacks `fopen → stat → opendir` pouvaient également perdre la vraie cause. Les appelants distinguent pourtant `ENOENT` d'une erreur de lecture pour éviter d'écraser un fichier existant.

Un backport restreint à **la source SDK exacte** est créé dans une copie de compilation isolée de la bibliothèque FS, par `tools/build_linux/prepare_fs_errno.py`. Le SDK installé n'est jamais modifié. La source originale est contrôlée par SHA-256 ; une copie isolée contenant des fichiers inattendus est refusée. Les clés de manifeste sont normalisées avec `as_posix()` pour éviter un échec au deuxième appel sous Windows ; 17 contrôles vérifient la copie réelle, son idempotence stricte et les formes de chemins Windows. Ce contrôle de chemins ne prétend pas exécuter Arduino sur Windows. Le rapport du build identifie les octets avant/après et le compilateur vérifie la bibliothèque effectivement sélectionnée.

Le backport :

- donne `ENOMEM` aux branches explicites de refus malloc/strdup, `ENODEV` à un montage absent et `EINVAL` aux entrées déjà reconnues invalides ;
- donne `EEXIST` à un mkdir visant un fichier existant ;
- conserve une cause non liée à l'absence à travers les fallbacks ; `ENOENT` n'est admis que si les échecs sont compatibles avec une absence ;
- si `stat` a confirmé la présence mais qu'une autre étape annonce une disparition, rapporte `EAGAIN` plutôt qu'une absence permettant une publication ;
- conserve l'erreur d'un `fflush` échoué après l'appel `fsync` existant. Un `fflush` explicitement négatif sans errno reçoit `EIO` ; ce remplacement n'est pas appliqué arbitrairement à un échec d'allocation.

La relecture indépendante a fait corriger deux erreurs de priorité dans une première proposition du backport : un premier `ENOENT` ne doit pas effacer un `EIO`/`ENOMEM`/`ENODEV` ultérieur ; un répertoire confirmé présent ne peut pas devenir une absence cohérente simplement parce qu'`opendir` échoue. Ces propositions intermédiaires ne sont pas le code final. La matrice de tests contient désormais les deux cas et exécute le vrai `tab5AtomicProbe`, pas seulement une imitation de sa décision.

Ce changement ne modifie ni l'ordre des appels d'E/S, ni les formats, ni les noms, ni les politiques de sauvegarde. Les fautes d'allocateur sans errno sont injectées pour vérifier un contrat défensif ; elles ne sont pas présentées comme une mesure de l'allocateur sur Tab5.

## Une seule classification des défauts de transport

`tab5StorageTransportError` centralise les quatre erreurs déjà reconnues comme réparables par remontage : `EIO`, `ENODEV`, `ENXIO`, `ETIMEDOUT`. Le signalement et le choix de nouvelle tentative du contrôleur utilisent cette même liste positive. Une liste négative partielle autorisait auparavant une nouvelle tentative automatique pour des erreurs telles que `EEXIST` ou `EINVAL`, que le remontage ne répare pas. Les erreurs de ressource, de chemin, de descripteur et de contention ne demandent pas un remontage.

Le journal ponctuel `m` indique maintenant séparément utilisateurs, fichiers SD, bail exclusif et disponibilité. Le seul nombre d'utilisateurs ne permettait pas de voir le blocage par une référence de fichier. Aucune interrogation périodique, nouvelle tâche ni écriture persistante n'est ajoutée.

## Exécutions réelles et limites

| Essai hôte avec SDK File/VFS réel | 0.8.44 | 0.8.45 |
| --- | ---: | ---: |
| Références pour un `File` persistant issu d'une ouverture échouée | 1 | 0 |
| Ce faux fichier bloque le contrôle de récupération | oui | non |
| Références après fermeture du pack cartographique devenu invalide | 1 | 0 |
| Références après 1 000 cycles de vraies copies/lectures/fermetures | 0 | 0 |
| Références après 400 ouvertures concurrentes de métadonnées | 0 | 0 |

La suite intégrée File/VFS exécute 5 034 contrôles pour chaque version. Le scénario courant utilise le backport SDK effectivement livré ; la référence utilise les octets SDK d'origine. La suite du backport exécute 76 contrôles avant/après, notamment `ENOENT` suivi d'une erreur de fallback et la conservation des fichiers reconnus présents. La régression du contrôleur et des remplacements atomiques conserve les scénarios de saturation, flush/close/rollback et 48 erreurs injectées.

ASan/UBSan actifs. Les sources SDK copiées conservent leurs licences et empreintes. String/Stream, mutex RTOS et appels POSIX constituent des frontières adaptées ; le test n'exécute pas FatFs, SPI, DMA ou une carte physique. LSan n'est pas utilisable dans cet environnement ; les références sont vérifiées explicitement. Les cycles ne constituent pas une mesure de latence, de tas libre ou de stabilité matérielle.

Preuves : `storage-sdk-0845.json`, `fs-errno-0845.json`, `storage-policy-0845.json`, `storage-target-0845.json` sous `tools/verification`. La compilation P4 de Storage et VolumeFS est réussie ; le build complet contrôle séparément l'origine liée de FS.cpp et vfs_api.cpp. Le contrôleur UI de récupération et ses essais intermodules sont détaillés dans le rapport de l'agent d'intégration.

## Points à vérifier sur Tab5

Après usage intensif des cartes, comparer deux commandes `m` sans redémarrage. Si les fichiers restent à zéro mais que la carte ne répond pas, conserver l'errno réel et le journal SPI/SD : la référence fantôme n'explique plus ce cas. Si des fichiers ou utilisateurs restent actifs, identifier la tâche propriétaire avant toute tentative de démontage. Tester aussi une lecture locale et la radio après une récupération ; ne pas formater ni réinitialiser les paramètres pour la reproduction.

L'audit ne justifie pas une nouvelle file globale d'E/S ou des délais supplémentaires. Les opérations existantes disposent déjà de baux et de files propres ; une référence invalide persistante ne disparaîtrait pas en ajoutant une file. L'interdiction de démonter sous un descripteur réellement actif reste nécessaire à l'intégrité des données.
