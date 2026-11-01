# Audit des applications et cycles de vie — 0.8.42

État initial : copie vérifiée de 0.8.41, avant correction des modules ci-dessous. Aucun essai sur Tab5 physique. Les limites et tailles indiquées proviennent du code, pas de mesures du tas sur appareil.

## Inventaire et référence

| Module | Exécution / stockage | Propriété et arrêt | Constat initial |
|---|---|---|---|
| ELF | Chargement sur UI ; application priorité 2, cœur 0, pile interne 16 Kio ou pile PSRAM déclarée au-delà de 16 Kio | Context PSRAM ; 32 allocations suivies ; fichiers bornés ; arrêt coopératif ; nettoyage UI après fin publiée ou suspension observée pour pile externe | Lecture jusqu’à 4 Mio, validation et relocation bloquent la tâche UI. La boucle de lecture seule impose jusqu’à 256 attentes d’un tick. |
| Affichage ELF | Deux tampons RGB565 PSRAM, jusqu’à 800 × 480 × 2 × 2 = 1 536 000 octets ; queue de 32 entrées en PSRAM | Ouverture depuis worker, création/destruction LVGL uniquement UI ; fermeture après fin du worker | Même mutex pour copie d’image de 768 000 octets, publication UI et entrées. L’UI utilise portMAX_DELAY. Contention confirmée, durée réelle à mesurer. |
| Panneau ELF générique | Modèle PSRAM, sortie et snapshot de 160 Kio chacun ; mutex court par commande | Worker copie les demandes/texte, UI crée les objets ; fermeture demande annulation, detach après fin réelle du worker | Copie UI de sortie pouvant atteindre 160 Kio uniquement sur changement de révision ; pas de tâche permanente. |
| MicroPython | Worker priorité 1, cœur 0, pile interne 32 Kio ; tas VM PSRAM 2 Mio ; source ≤ 64 Kio en PSRAM | Arrêt coopératif ; libération VM/source/services/matériel avant active=false ; console PSRAM 8 Kio retenue | Pas de LVGL direct depuis worker. Lecture initiale et copie de source synchrones ; console retenue après fermeture. |
| Modèle UI Python | Modèle de widgets PSRAM alloué à la demande, mutex ; queue de 16 événements | Worker modifie modèle, UI copie ; génération et révisions ; releaseStorage à la fin VM | Aucun callback LVGL worker identifié. Publication de fin image sans génération à investiguer si une opération ancienne peut dépasser une nouvelle session. |
| Cartes | Worker réseau à la demande, queues à payload PSRAM ; tables/cache PSRAM | Annulation par génération, vidage des résultats ; fermeture réseau avant publication de fin du worker | Chargement initial styles/lieux/packs sur UI ; coût dépend de la SD. Les durées réseau ne bloquent pas directement l’UI. |
| Export JPEG | Worker priorité 1, cœur 0, pile 16 Kio ; lignes de travail et encodeur PSRAM | Bail stockage, sortie temporaire validée puis renommée ; annulation, libération sur les branches d’erreur examinées | Pas de tâche permanente. Les attentes par ligne ont lieu sur worker. |
| Dessin natif | LVGL/UI ; travaux de sélection/remplissage incrémentaux par ie_job_step(...,4096) | Fermeture refuse le travail actif ; annulation explicite et destruction des ressources UI/document | Le nombre de pixels par étape borne le travail mais ne garantit pas une durée maximale sur matériel. |

Le code natif ELF partage l’espace d’adressage du firmware. La validation du fichier et le contrôle des API ne constituent **pas** une isolation mémoire ou une protection contre une boucle native non coopérative.

## Priorités établies

1. P1 : enlever l’attente de la copie d’image dans le service graphique et séparer la synchronisation des entrées, sans perte des événements.
2. P1 : sortir lecture/validation/relocation ELF de la tâche UI, avec publication explicite et maintien du nettoyage UI.
3. P2 à confirmer : coût des ouvertures SD Cartes/Python et durée des étapes Dessin sur appareil.

## Corrections appliquées

### Tab5NativeView.cpp — contention de copie et entrées

Le service UI tente maintenant le mutex des images sans attente. Si le producteur écrit encore le tampon arrière, la dernière image reste en attente jusqu’au passage suivant. La permutation et les compteurs sont protégés ; les appels LVGL ont lieu après libération du mutex. La queue d’entrées dispose de son propre mutex, limité à push/pop bornés. Les événements clavier et boutons ne sont plus couplés à la copie de 768 000 octets (750 Kio) d’un canvas 800 × 480. Aucun tampon, fréquence de rafraîchissement ou mécanisme de perte d’événements n’a été ajouté.

### Tab5Executable.cpp/.h — chargement hors de la tâche graphique

`tab5ExecutableRun` valide et copie la demande, réserve la pile puis rend la main. La même tâche interne de 16 Kio qui exécutait auparavant l’ELF lit maintenant le fichier, valide et relocalise l’image avant d’exécuter son point d’entrée. Les ELF demandant une pile supérieure conservent leur tâche à pile PSRAM ; le chargeur interne se termine après transmission. La boucle de lecture vérifie désormais l’annulation entre blocs.

`launching` est une publication atomique : l’UI ne lit pas le contexte ou le cache console partiellement construits. Les réservations depuis le worker utilisent le mécanisme existant de collecte sur l’UI. Une erreur, y compris après allocation du contexte ou relocation partielle, est publiée au service UI, seul propriétaire de la destruction LVGL et du contexte. Les allocations temporaires de lecture et la demande sont libérées par le chargeur avant publication. L’arrêt de l’application conserve la barrière existante : fin de l’audio et du matériel, puis publication de fin, ou suspension observée avant suppression SDK des piles externes. Le handle externe n’est plus réécrit par le worker après publication par le créateur.

L’API `tab5ExecutableTakeLaunchError()` signale une fois une erreur de chargement différée. La boucle principale ouvre le formulaire/console existant, qui affiche `Status` et `SourcePath` sans relancer l’ELF. Une annulation reste une annulation, pas un message de réussite ni d’erreur. Aucune sauvegarde automatique n’a été introduite.

### Tab5Executable.cpp / Tab5AtomicFile.h — intégrité des sauvegardes ELF

`appSave` emploie les sondes et le remplacement communs corrigés par le lot stockage. Une erreur de stat distincte de ENOENT ne vaut plus absence. Les récupérations `.old`, la suppression d’un temporaire existant et le remplacement sont vérifiés avant de continuer. Les erreurs errno de `flush` puis de `close` sont capturées immédiatement, séparément, avant promotion ; le SDK ne les reporte pas dans `getWriteError`. Une promotion échouée tente de restaurer le fichier d’origine et conserve `.old` si la restauration échoue. Les noms de fichiers, extensions autorisées, limites de taille et choix de sauvegarde des jeux restent identiques. Après succès confirmé, l’ancien nettoyage du `.old` reste conservé ; un échec de ce nettoyage laisse une copie plutôt que d’invalider la nouvelle sauvegarde.

### Tab5ImageWrite.h / Tab5Jpeg.cpp — écriture des images

Les deux instanciations de production du writer sont `fs::FS`/`File` dans Dessin et dans l’ABI ELF. Le template reste indépendant d’Arduino ; son adaptateur de fichiers distingue maintenant ENOENT des erreurs EIO avant récupération, création du temporaire et remplacement. Les doubles de fichiers des tests déclarent explicitement ce contrat errno. Les chemins et noms `.tab5tmp`/`.tab5old`, les formats autorisés et le choix utilisateur de sauvegarder restent inchangés.

Le writer et l’export JPEG capturent séparément errno immédiatement après `flush` et après `close`. Une erreur de l’une de ces opérations empêche la promotion. JPEG distingue aussi une destination temporairement illisible d’une destination inexistante, au début et avant renommage final. L’encodage et la vérification de longueur/marqueurs JPEG restent présents. Le défaut SDK est reproduit sur la version originale avec un `flush` ou `close` en EIO sans indicateur d’erreur d’écriture : les deux chemins validaient à tort la sortie.

## Résultats mesurés et ressources

| Mesure / contrainte | 0.8.41 | 0.8.42 | Portée |
|---|---:|---:|---|
| Service image sous copie artificiellement retenue 80 ms | 80,709 ms | 0,002 ms | Test hôte déterministe ; pas une mesure ESP32 |
| Envoi d’une entrée sous la même contention | 80,463 ms | 0,002 ms | Test hôte ; événements conservés |
| Lecture ELF de 4 Mio | 1024 lectures et 256 attentes explicites sur UI | mêmes lectures/attentes sur worker | Vérifié par les fonctions exactes avec FS injecté |
| Double tampon canvas maximal | 1 536 000 octets PSRAM | identique | Taille issue du code |
| Pile interne ELF ordinaire | 16 Kio | même pile pour chargeur + exécution | Pas de seconde tâche normale |
| Demande de lancement | variables locales UI | 776 octets PSRAM temporaires | Libérés avant exécution/publication d’échec |
| ELF à grande pile | pile PSRAM déclarée | identique + chargeur interne 16 Kio temporaire | Surcoût transitoire à mesurer sur appareil ; admission explicite |
| Synchronisation NativeView | 1 mutex statique | 2 mutex statiques | Un mutex court supplémentaire ; aucun tampon permanent supplémentaire |

Le système de fichiers de test autorise le remplacement d’une destination à la manière POSIX : sur la version originale, une erreur EIO de stat du fichier principal est traitée comme une absence, la sauvegarde annonce un succès et détruit la copie de secours. Le même scénario est désormais refusé avant modification du principal, du `.old` ou du temporaire.

Les chiffres du tas interne au démarrage, les temps de démarrage réels, les performances PSRAM et la latence tactile effective ne sont pas mesurables par ces tests hôte. La compilation cible et le bilan global Flash/RAM figurent dans le rapport de construction.

## Tests effectivement exécutés

- `test_native_view.py` : fonctions de production `beginMode`, `present`, service de publication, queue, `inputPoll` et `close` ; queue réelle, LVGL/allocateur/capacités simulés. Comparaison originale/corrigée sous contention ; intégrité des pixels, conservation des entrées, 128 ouvertures/fermetures et les trois échecs d’allocation partielle de chaque cycle. Aucun bloc suivi restant.
- `test_elf.py --loader` : 769 cas sur les fonctions exactes de demande, lecture, validation/relocation, publication, exécution et nettoyage ; erreurs SD/admission/allocations/relocation/cache, arrêt avant lecture, pendant lecture/validation et avant entrée, échec de tâche et transfert vers pile externe. Service UI injecté pendant les phases sensibles et immédiatement après publication. Aucun fichier, bail stockage ou bloc suivi restant. La relocation/validation et les frontières SDK sont simulées ; aucun binaire ELF exécuté par l’hôte.
- `test_elf.py` : 1024 cycles sur le worker et le service exacts, succès/erreur/annulation/audio/argument, nettoyage immédiat au point de publication, état externe running/blocked/suspended, libération audio après zéro à trois échecs injectés. Aucune libération de contexte avant restitution de l’audio.
- `test_elf_save.py` : reproduction du défaut original et 85 scénarios corrigés de sondes EIO, récupération et points d’échec de lecture d’état, écriture, flush, close, suppression ou renommage. Chaque échec conserve le contenu original dans le principal ou `.old` ; succès = nouveau contenu principal. Les erreurs errno de flush/close sont injectées sans writeError, conformément au SDK.
- `test_image_io.py` : wrapper JPEG de production et encodeur jpge réel, relecture des marqueurs et de la longueur, 30 points d’erreur I/O et échecs d’allocation/sondes/flush/close. Writer de production : 35 points de transaction plus erreurs ciblées de sondes et de flush/close. Suites complètes existantes writer/ABI6 réexécutées (ouverture/fermeture répétée, annulation, formats, limites, petits transferts, queue). Les décodeurs PNG/WebP/BMP n’ont pas changé et leur suite complète n’a pas été rejouée dans ce lot ; ces tests ne prétendent pas en couvrir toute l’implémentation.

La compilation cible finale des unités `Tab5Executable.cpp`, `Tab5NativeView.cpp` et `Tab5Jpeg.cpp`, incluant le writer générique côté ELF, a réussi avec le SDK M5Stack 3.3.9 / GCC RISC-V 14.2. Les empreintes finales concordent (`apps-target-0842.json`). Les plus grands cadres de pile statiques indiqués par GCC sont 656 octets pour appSave, 192 pour NativeView::service et 224 pour encode JPEG ; ce ne sont pas des maxima cumulés de pile de tâche. La compilation complète, incluant l’instanciation Dessin et l’édition des liens, appartient au rapport de construction global.

Tous ces exécutables ont tourné avec AddressSanitizer et UndefinedBehaviorSanitizer. LeakSanitizer ne fonctionne pas sous le ptrace de cet environnement ; `ASAN_OPTIONS=detect_leaks=0` est donc explicite. Le suivi des allocations/fichiers/baux pertinent pour chaque scénario remplace ici la recherche LSan, sans prétendre mesurer le tas ESP32. Les JSON joints contiennent les empreintes des sources testées. Une revue indépendante du lot mémoire n’a relevé aucun défaut nouveau dans la publication du contexte, le transfert des piles ou la fermeture NativeView.

## Audit complémentaire et résiduels

| Criticité | État | Point / investigation |
|---|---|---|
| P1 | Limite architecturale, inchangée | Un ELF natif non coopératif peut monopoliser un cœur ou corrompre le firmware. Aucune MPU/PMP ou isolation par application n’est configurée par ce chargeur. Tester seulement les ELF de confiance ; la validation de format ne constitue pas un bac à sable. |
| P1 | À mesurer sur matériel | Déterminer la marge réelle de la pile de 16 Kio pendant validation/relocation, désormais sur worker. Les points `[STACK] chargement ELF` et `[STACK] application ELF` sont conservés/ajoutés pour cela. Pour les grands ELF, mesurer le pic interne temporaire du chargeur. |
| P2 | Coût confirmé, impact matériel à quantifier | Cartes charge initialement styles, lieux et métadonnées de packs hors ligne sur UI ; lecture du cache et rasterisation de certaines tuiles y ont aussi lieu. Les ouvertures Python lisent ≤64 Kio puis recopient la source. La pagination du lecteur Python mesure des labels LVGL et lit la SD sur UI. Ces coûts ne prouvent pas un lag au repos ; tracer leur durée avec le diagnostic de boucle. |
| P2 | Limites inspectées, pas de fuite démontrée | Cartes : pile worker 16 Kio, priorité 2 ; cache 24 tuiles, plafond 2 Mio ; 128 métadonnées de packs, jusqu’à 2 Mio d’index d’un pack. Fermeture : annulation par génération, vidage des résultats et fermeture sockets avant publication du worker. Réactivation concurrente garde le worker existant. |
| P2 | Limites inspectées | Dessin : jobs de sélection/remplissage progressifs à 4096 unités par service ; masque/layers/undo restent volontairement en mémoire quand on cache l’app. Le preview est libéré au masquage ; fermeture explicite garde la confirmation du document modifié. La durée maximale d’une unité n’est pas garantie par le nombre 4096 : mesurer sur grande image/filtres. |
| P2 | Aucun accès invalide identifié dans les chemins examinés | Capture carte garde un pointeur de canvas pendant export ; quitter Cartes change page/génération, et MapPrint vérifie ces deux valeurs avant tout nouvel accès. Son service libère le pointeur et le fichier temporaire à l’annulation. Les appels restent sur UI. Export JPEG libère encodeur et lignes PSRAM sur les branches examinées ; son helper synchrone n’a pas de caller dans cette version. |
| P2 | Hypothèse à reproduire | La fin d’image demandée par Python ne porte pas de génération, contrairement au lecteur de documents. Vérifier arrêt d’un script avec visionneuse ouverte, nouveau script puis fermeture de cette visionneuse. Aucun changement spéculatif de ce protocole n’a été introduit. |
| P2 | Limite historique | `extendedControls` est un bool partagé : les ELF embarqués déclarent leurs commandes avant `ready`, mais un ELF tiers qui le modifierait après démarrage vidéo ne respecte pas ce séquencement. La publication atomique d’un mode dynamique et son effet UI demanderaient un contrat explicite. |
| P2 | Contrat à respecter | Le panneau ELF générique renvoie un pointeur vers le rapport terminé pour export UI. Les ELF fournis terminent leur écriture avant l’état DONE ; une écriture tardive d’un ELF tiers pourrait modifier ce rapport pendant export. Ce n’est pas une isolation mémoire. Mesurer aussi la copie du snapshot 160 Kio sur changement de révision. |
| P3 | Coût volontaire | Console Python 8 Kio PSRAM conservée pour consultation après fin ; cache console ELF 16 Kio PSRAM évictable lorsque inactif. Ni l’un ni l’autre ne justifie à lui seul une pénurie de SRAM. |

Le modèle Python UI utilise un mutex, une queue de 16 événements et des générations ; il ne manipule pas LVGL depuis le worker. La VM restitue tas, source, modèle UI, services et matériel avant publication d’arrêt. La réacquisition du bail SD après récupération peut attendre l’exclusivité du stockage, même après annulation ; l’attente se situe sur worker et n’est pas une preuve de deadlock tant que les récupérations bornées terminent. Les tests globaux du lot stockage couvrent l’exclusivité, pas un essai physique de cette séquence Python.

## Vérification courte sur Tab5

1. Ouvrir une application ELF avec un fichier proche de 4 Mio en déplaçant continuellement la souris. Vérifier que le pointeur reste servi pendant « Chargement », puis arrêter pendant le chargement et relancer. Capturer les lignes `[STACK]` et `[MEM]`, ainsi que les dépassements de boucle UI.
2. Alterner 20 fois une application ELF graphique ordinaire et une application utilisant une grande pile (par exemple l’ELF Doom fourni), quitter via l’application, attendre la ligne de contrôle mémoire différé. Comparer libre/minimum/plus grand bloc interne et PSRAM ; aucun recul permanent inexpliqué ne doit s’accumuler.
3. Dans un canvas natif 800 × 480, maintenir les mouvements de souris, touches et boutons pendant un rendu soutenu. Vérifier ordre des touches, absence de clic perdu et poursuite de l’affichage. Les mesures hôte ne garantissent pas la fréquence d’image physique.
4. Sur des **copies** de sauvegardes de test, vérifier sauvegarde/relecture puis récupération d’un `.old` après absence du principal. Ne pas provoquer une coupure sur des données réelles pour ce test. Les défauts EIO/rollback sont déjà injectés côté hôte ; une robustesse face à toute coupure physique n’est pas démontrée.
5. Ouvrir/fermer Python, Cartes hors ligne et Dessin, et mesurer les pics de service sur les opérations longues ciblées ci-dessus. Pour Dessin, vérifier que masquer/restaurer conserve les modifications et que fermer propose toujours les choix d’enregistrement habituels.

