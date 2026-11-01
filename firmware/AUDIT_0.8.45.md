# Tab5OS 0.8.45 — audit SD, radio, affichage et navigation cartographique

Référence : livraison 0.8.44 conservée intacte, SHA-256 `b5fdc983e51e17568d700b9ae1359e902fa61f43c86505a775e650f10cab16aa`. Cette correction traite des défauts reproduits dans les sources actuelles et les dépendances exactes. **Aucun essai sur Tab5 physique : les flashs Dessin/Média restent un problème ouvert.**

## Ce qui est établi

| Incident | Cause vérifiée | Correction et limite |
| --- | --- | --- |
| Reconnexion SD impossible après Cartes | Le SDK renvoie parfois un pointeur d'implémentation non nul mais un fichier invalide. VolumeFS le retenait comme fichier ouvert ; Cartes omettait de fermer son membre invalide. Le contrôleur restait bloqué avant `SD.end()` | Vérification de validité réelle à l'ouverture/énumération et fermeture inconditionnelle du pack. Test bout en bout avec FS/VFS réels : référence persistante 1 → 0, chemin de remontage débloqué. La disparition physique de la SD reste à mesurer |
| Diagnostic de récupération trompeur | Le probe ne vérifiait pas correctement les échecs de synchronisation et de fermeture ; le retry utilisait une erreur globale écrasable par un autre worker | Probe vérifié jusqu'à la relecture, cause locale conservée, classification transport commune. Les travaux actifs restent protégés ; aucun remontage sur une simple saturation ou collision |
| Lieux enregistrés exposés aux erreurs SD | Un échec de lecture ou de JSON pouvait laisser une liste vide considérée comme chargée, puis écrasée par un ajout | Chargement validé avant publication ; ancienne mémoire conservée sur erreur ; sauvegarde interdite tant que l'état reste inconnu. Relecture avant mutation, backup conservé |
| Radio intermittente/reset | Le client HTTP pouvait survivre à son transport dynamique après déconnexion. Défaut de durée de vie reproduit sous ASan avec le SDK exact | Client HTTP détruit avant le transport. Les autres propriétaires HTTP ont été vérifiés. L'absence totale de reset sur tablette n'est pas déduite de ce test |
| Attente radio perturbant les tâches | Les en-têtes fragmentés pouvaient être attendus dans la boucle SDK sans rendement explicite | Utilisation du transport coopératif existant. Les appels DNS/TCP/TLS déjà engagés gardent les délais du pilote |
| Claquement MP3 au démarrage | Redémarrage supplémentaire I2S/codec à la première fréquence MP3 et à chaque changement de fréquence source, malgré le rééchantillonnage natif M5Unified | Horloge de sortie configurée conservée, fréquence source transmise par tampon. Redémarrage supplémentaire 1 → 0 dans le test. Le rôle acoustique exact reste à vérifier |
| Zoom avant fin de chargement | Aucun défaut d'ownership démontré dans la génération/tampon avec cette seule action ; les recherches de fichiers manquants exposaient cependant le défaut VolumeFS | Zoom libre conservé, aucune attente de fin des tuiles. Tests de requêtes périmées et de tuiles déjà possédées par l'UI, sans verrou supplémentaire |
| Recherche des lieux | Le comportement précédent privilégiait le local | Internet d'abord lorsque disponible, puis local après erreur ou résultat vide. Hors ligne ou réseau détenu par une radio : local disponible directement |

## Pourquoi les contrôles précédents ne suffisaient pas

Le modèle de stockage précédent assimilait une ouverture ratée à un pointeur nul. Il ne représentait pas la distinction réelle entre `FileImplPtr` non nul et `File::operator bool()` faux. Il pouvait donc valider des compteurs et fermetures sans exposer la référence qui bloquait réellement le contrôleur de reconnexion.

La correction vidéo de la 0.8.44 avait traité une durée de vie HTTP, mais l'implémentation radio reproduisait ce défaut. Cette version examine tous les propriétaires HTTP du projet : Print, Python/Archives, NetworkHttp et Map détruisent leurs objets dans le bon ordre ; Radio nécessitait la même correction de propriété que Video.

Les nouveaux contrôles exécutent les implémentations FS/VFS exactes du SDK, les fonctions complètes du contrôleur principal et les méthodes HTTP précises en cause. Les frontières SPI/FatFs, réseau, RTOS et matériel restent adaptées : l'exécution du firmware entier sur la tablette n'est pas simulée comme si elle avait eu lieu.

Une relecture indépendante du nouveau backport FS a aussi relevé une priorité d'erreurs pouvant produire un faux `ENOENT`. Elle a été corrigée et retestée avant livraison. La règle finale n'autorise l'absence que lorsque les échecs des recherches successives sont compatibles avec celle-ci ; une E/S défaillante, un manque de mémoire ou une présence contradictoire restent des erreurs.

## SD : propriété, état et récupération

Une file supplémentaire ne résout pas une référence conservée indéfiniment. Le cache cartographique possède déjà un résultat en attente côté UI ; ses écritures sont synchrones dans cette tâche et la disponibilité connue du moteur est contrôlée avant le cache. Aucun défaut d'écriture avant montage n'a été démontré dans ce chemin.

VolumeFS retient désormais le support uniquement pour une implémentation réellement ouverte. La fermeture de l'offline pack fonctionne aussi lorsqu'un changement de génération rend le `File` faux. La récupération libère les itérateurs UI et le sélecteur avant le bail exclusif, puis attend les propriétaires actifs et la fermeture de leurs fichiers. Un fichier audio lu directement par la bibliothèque SD reste protégé par le bail du worker et `storageIdle()` ; il n'est jamais invalidé pour forcer un remontage.

Le probe conserve sa propre cause d'échec : un worker refusé pendant l'exclusivité peut publier `EBUSY`, mais cette dernière erreur de diagnostic ne doit pas supprimer une reprise nécessaire de l'EIO du probe. Le test reproduit cette interférence avec un vrai thread, puis vérifie qu'elle n'affecte plus la décision. Les erreurs de transport existantes `EIO`, `ENODEV`, `ENXIO`, `ETIMEDOUT` gardent leurs reprises bornées ; les autres causes ne lancent pas une nouvelle récupération de bus.

Le backport FS est limité aux erreurs : allocations connues → `ENOMEM`, paramètres/montage explicites, échec `fopen`/`stat`/`opendir` cohérent et premier échec `fflush` conservé. Aucun ordre d'E/S, format de données, délai ou politique de montage n'est changé. Il s'applique à une **copie isolée** du SDK 3.3.9 exact, contrôlée par empreinte. Le SDK installé reste intact. La compilation vérifie que la source corrigée, ses objets et le lien final sont réellement utilisés ; les chemins Windows sont raccordés au même helper, sans prétendre les avoir exécutés.

## Cartes : zoom et recherche

Le changement de zoom invalide les anciennes requêtes et vide leurs résultats détenus par les queues. Le worker vérifie la génération avant publication. Une tuile déjà transférée à l'UI reste la propriété de cette UI jusqu'à son écriture/libération ; changer la vue ne libère pas son tampon. Son cache est identifié par style/zoom/x/y, indépendamment du canevas courant. Le dessin d'une réponse n'est autorisé que pour la génération actuelle.

Les essais couvrent 50 changements de génération pendant un téléchargement, un résultat en attente côté UI pendant 40 autres générations et la libération unique des anciens résultats. Le test LVGL vérifie aussi 100 changements de zoom avec des tuiles déclarées en attente. Ces essais ne mesurent ni SPI ni la latence d'une écriture sur une carte réelle.

La recherche normale demande Internet en premier et vérifie la connexion SDK au moment du travail. Erreur HTTP/TLS, déconnexion ou résultat vide déclenchent le local si la requête est toujours actuelle. Une requête périmée ne relance pas un parcours SD. La radio ne doit pas rendre le hors-ligne inaccessible : quand son bail exclusif empêche Internet, le local est utilisé sans attente indéfinie. Les codes de l'échec en ligne sont journalisés avant le repli, sans URL ni requête contenant des données personnelles.

## Affichage : problème restant ouvert

Le chemin Dessin, les changements de volets Média, les tampons, cache, rotations et fonctions DSI ont été examinés. L'essai actuel utilise les routines de soumission et de callback DMA du SDK exact, avec vérification de leur provenance et de leur correspondance au désassemblage. Aucun défaut logique de réutilisation du framebuffer n'y a été reproduit.

Le pilote Espressif associe explicitement un underrun à un affichage bleu. Les déplacements de grosses données PSRAM et le scan DSI peuvent se concurrencer, mais **aucun compteur fourni par la tablette n'établit encore cette cause**. Les contrôles de reset LCD et d'expandeur I²C ne montrent pas de remise à zéro LCD périodique dans le code actuel.

Aucun changement aveugle de fréquence, QoS, reset, fence, buffer ou rendu n'est introduit. La commande `d` ajoute seulement la configuration réellement lue : fréquence demandée, dimensions, rotation et timings. Elle complète les compteurs de soumission et d'underrun sans timer ni mémoire permanente. La fréquence demandée n'est pas une mesure de fréquence effective.

## Validation, intégrité et limites

Voir [VERIFICATION_0.8.45.md](VERIFICATION_0.8.45.md) pour compilation, source exacte du BIN, dépendances, diagnostics de compilation et empreintes. Les tests actuels sont séparés des preuves historiques. ASan/UBSan sont actifs ; LSan n'est pas exploitable ici, les ressources suivies sont contrôlées explicitement.

Les applications, fonctionnalités, données, secrets, réglages, options d'enregistrement/autosauvegarde et partitionnement sont conservés. Aucune sauvegarde de document forcée, nouvelle restriction C6 ni exclusion d'ELF n'est ajoutée. Les deux cœurs P4 restent utilisés par l'architecture existante ; le C6 n'est pas assimilé à une extension du tas P4.

Les flashs, la récupération physique SD, l'acoustique MP3 et la stabilité radio restent à tester sur Tab5. Une vraie pénurie interne/contiguë peut encore refuser une application malgré la PSRAM libre ; aucune marge de sécurité n'est diminuée. Certaines E/S restent synchrones et les ELF ne possèdent pas d'isolation matérielle de processus. La livraison ne constitue pas une déclaration de stabilité matérielle complète.

Détails par module : `tools/audit-0845/storage.md`, `integration-review.md`, `maps.md`, `audio.md`, `display.md`, `storage-peer-review.md`.
