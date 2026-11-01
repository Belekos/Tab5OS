# Média : publication du catalogue et mises à jour LVGL

## Cause établie

La 0.8.46 possède déjà un pool de 24 lignes, mais `refreshAudioList()` annule ce bénéfice à chaque reconstruction du modèle : toutes les lignes sont cachées, l'étendue de défilement est remise à zéro, le focus est supprimé, les styles sont réappliqués, puis les lignes sont montrées de nouveau. Le titre et les options de tri sont également réécrits même lorsqu'ils sont identiques. Cela ne démontre pas la cause de chaque flash physique ; le coût graphique existe indépendamment du pilote DSI.

Les quatre boutons de catégorie étaient aussi `CHECKABLE`. LVGL inversait leur état lors du relâchement, avant le callback. Le callback de l'onglet courant quittait immédiatement : le bouton actif pouvait donc rester visuellement décoché. Ce défaut est reproduit avec le vrai gestionnaire d'événements LVGL.

Un second défaut concernait l'identité des éléments : les lignes conservaient des indices numériques alors qu'un rechargement ou la fin d'un enregistrement pouvait remplacer le catalogue. Conserver ces références après un échec de reconstruction pouvait faire lire ou supprimer un autre fichier. Vérifier l'indice uniquement au clic ne suffisait pas : une commande audio peut attendre dans sa file pendant un autre rechargement.

## Changements

- Le modèle de références est préparé dans un second tableau PSRAM réutilisable. L'ancien catalogue, ses pixels et son défilement restent affichés pendant la préparation. `show()` publie le modèle complet ; `cancel()` conserve le précédent. Les actions sont refusées pendant la préparation et un clic commencé avant celle-ci ne peut pas activer un nouvel élément.
- L'interface rétablit son contexte de navigation et ses groupes si la préparation ou le tri échoue. Elle conserve les anciennes références seulement si leur catalogue source reste identique. Sinon, les références périmées sont retirées ; une nouvelle tentative explicite est possible. Un échec mémoire n'entraîne pas de reconstruction répétée à chaque tour de boucle. La préférence globale de tri n'est pas écrasée par un échec d'affichage ; le sélecteur reflète l'ordre encore affiché et la tentative suivante utilisera la préférence demandée.
- Trois révisions indépendantes protègent les pistes, les stations et les vidéos. Une mutation marque temporairement sa révision indisponible ; la construction vérifie la même révision avant et après lecture. Les actions vérifient aussi les copies utilisées pour favoris, suppression et vidéo. Les commandes audio transportent leur révision jusqu'à la résolution sous verrou dans le worker, y compris après report pour priorité mémoire. Un indice périmé est refusé avec les messages traduits existants.
- La reprise radio conserve son URL et la lecture locale son chemin. Après insertion, suppression ou réorganisation du catalogue, reprise et précédent/suivant résolvent cette identité avant de choisir un voisin ; un élément disparu n'est pas remplacé silencieusement par celui qui occupe son ancien indice.
- Les lignes conservées ne réécrivent que leurs textes modifiés. Police, neuf couleurs et rayon sont comparés exactement avant une réapplication des styles. Les vrais changements de thème restent pris en compte.
- Les boutons de catégorie sont pilotés uniquement par la vue publiée, sans bascule automatique concurrente de LVGL.
- Le tri conserve une lecture de métadonnées par élément, hors comparateur. Ses noms sont regroupés dans une arène PSRAM extensible plutôt qu'une allocation par nom. L'échec de n'importe quelle allocation ne modifie pas l'ordre précédent.
- Cinq anciens callbacks Média sans aucun appel ont été supprimés (`audioListItemEvent`, `audioAlbumEvent`, `audioRadioCategoryEvent`, `audioDeleteRecordingEvent`, `audioFavoriteEvent`). Les helpers de titres utilisés ailleurs sont conservés. Aucun gain binaire n’est attribué à ce nettoyage : le compilateur pouvait déjà éliminer ces fonctions statiques.
- Destruction et récupération libèrent les deux tableaux et tous les objets du pool. Aucun timer, thread ou changement de cœur n'est ajouté.

## Mesures exécutées

Le test compile les sources de production avec LVGL 9.6.0 réel, son rendu logiciel et ASan/UBSan. La référence provient du ZIP livré 0.8.46, SHA-256 `22d99628cc3ab6a9673be1886755ec9a3092aaede3f017a7e0e20443cc0816a6`. Les fonctions de navigation et de reconstruction de la page, ainsi que les blocs de création des quatre onglets, sont extraits sans réécriture du source principal puis compilés avec le vrai modèle et LVGL. Les frontières audio, catalogue, stockage et session sont contrôlées dans ce second test.

| Scénario | 0.8.46 | 0.8.47 |
|---|---:|---:|
| Pixels rendus lors de 100 publications de catalogue identique | 27 000 000 | 0 |
| Pixels rendus par une présentation forcée pendant une préparation incomplète | 247 044 | 0 |
| Pixels rendus lors de 40 changements de catalogue | 9 882 096 | 4 138 774 |
| Allocations temporaires explicites pour trier 800 éléments | 801 | 8 |
| Lectures de métadonnées par tri de 800 éléments | 800 | 800 |
| État d'onglet incohérent après 80 clics sur l'onglet actif | 80 | 0 |
| Catalogue et contexte précédents conservés après échec de remplacement, source inchangée | Non | Oui |
| Octets des tableaux de références résidents pour 800 éléments | 8 192 | 16 384 |
| Pic des allocations PSRAM suivies dans ce scénario | 50 592 | 64 768 |
| `sizeof(Tab5MediaList)` compilé pour P4 | 908 | 1 056 |
| Allocations explicites encore vivantes après destruction | 0 | 0 |

Le coût mémoire supplémentaire est volontaire et borné : un second tableau de références, en PSRAM, permet de ne pas détruire le résultat valide avant que son remplaçant soit prêt. Le plafond de 8 192 références reste inchangé. Au maximum, les deux tableaux occupent 128 Kio au total. L'objet lui-même grossit de 148 octets, mesurés avec le compilateur P4 réel. Il est créé à la demande et détruit par la récupération du catalogue masqué.

Les tests couvrent aussi navigation clavier au-delà du pool, favori, tri par taille, absence de lecture SD pendant le défilement, clic périmé, échecs à chaque allocation du tri, échec d'agrandissement du modèle, changement de thème et destruction. Les compteurs d'assertions de référence et de version corrigée ne doivent pas être additionnés comme un nombre de scénarios indépendants : le nombre de points d'injection d'échec diminue avec les allocations.

Les 220 vérifications de l'intégration Média corrigée couvrent notamment un rechargement suivi d'un échec mémoire avant suppression d'enregistrement, une mutation pendant la construction, une mutation pendant la copie d'une sélection et une seconde mutation lors de la copie vidéo. Aucun autre fichier n'est activé ou supprimé. Le helper réel de révision est également exercé sur 20 000 mutations concurrentes sous verrou, des compteurs indépendants et le débordement du compteur.

Le test audio complémentaire compile les méthodes réelles de file, d'exécution, de démarrage, de reprise et de voisinage. Il passe 921 vérifications par variante comparée, dont les commandes périmées, la réorganisation entre sélection et exécution, la reprise après réindexation et le retrait de l'élément courant. Les frontières réseau, codec et transport FreeRTOS sont contrôlées ; ce résultat n'est pas un essai de diffusion radio physique. Voir `tools/verification/audio-catalogue-0847.json`.

## Limites

Ces chiffres sont des pixels calculés par LVGL et des allocations suivies sur hôte, pas une mesure de bande passante PSRAM physique ni une preuve de disparition des flashs. Les allocations ordinaires de LVGL/libc ne sont pas incluses dans le suivi explicite PSRAM ; LSan est désactivé dans cet environnement, ASan et UBSan restent actifs. Aucun périphérique Tab5 n'a été testé ici.

Le banc d'intégration extrait les corps des fonctions, sans reproduire le préprocesseur de prototypes Arduino. La compilation complète a identifié puis permis de corriger deux déclarations : le type de retour de `audioRememberItem` devait devenir `bool`, et le helper prenant `AudioView` devait disposer d'un prototype explicite après cet enum. Les corps testés n'ont pas changé pour cette correction ; la compilation complète reste nécessaire pour contrôler cette frontière.

Le chargement initial du catalogue, l'énumération des sous-dossiers et les métadonnées taille/date sont encore synchrones dans la tâche UI. La publication transactionnelle évite de vider l'affichage avant leur résultat, mais ne les transforme pas en opérations asynchrones. Une migration ultérieure vers un worker doit conserver les révisions d'identité et rendre explicite la propriété des résultats. Les petits compteurs de révision, le champ de commande et l'URL retenue ajoutent aussi de la mémoire ; les chiffres du tableau concernent le modèle de liste, pas ces objets moteur ni le bilan global du firmware.

Reproduction : `python3 tools/media-ui-tests-0847/run.py`, après préparation du LVGL hôte selon les chemins indiqués dans le script. La taille P4 est reproductible avec `size_probe.py` et la base de compilation cible. Rapports : `tools/verification/media-ui-0847.json`, `media-ui-target-0847.json`, `media-ui-size-0847.json`, `media-catalogue-target-0847.json`. La compilation complète reste la preuve d'intégration de tous les modules.
