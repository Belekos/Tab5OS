# Diagnostic ciblé — Tab5OS 0.8.43

## Référence et portée

Le projet est repris directement du ZIP 0.8.42 livré, SHA-256 `c7e5fc18494b2cf8be7ad91527d2da6a053717e8138902404e2030e4e54cd927`. L’archive est conservée intacte ; ses sources et preuves sont inscrites dans le manifeste de référence. Aucun code n’a été repris d’une ancienne version supposée. Les résultats hôte établissent des propriétés du code, pas les performances physiques de la Tab5.

Cette correction cible le lag de l’accueil, le travail périodique évitable, les exclusions anciennes d’applications et l’organisation mémoire. Les fonctions, fichiers utilisateurs, secrets, préférences, migrations nécessaires et choix d’autosauvegarde sont conservés. Aucun changement de partition, de firmware C6, de chiffrement ou de tension périphérique n’est introduit.

## Causes confirmées et corrections

| Constat de référence | Correction | Ce qui est effectivement vérifié |
|---|---|---|
| Même avec uniquement les applications intégrées affichées, l’accueil relançait toutes les cinq secondes l’énumération SD/USB/Python. | Scans demandés à l’entrée des bibliothèques et selon révisions utiles ; aucune énumération de bibliothèque à l’accueil natif au seul passage du temps. | Vraies fonctions Home/Catalogue et LVGL : sur 120 s du scénario, 5 333 ouvertures de fichiers →0, 5 302 appels d’énumération →0. Ce ne sont pas des millisecondes SD mesurées. |
| Une ouverture de bibliothèque ou son actualisation lisait le stockage depuis le chemin UI. | Worker ponctuel cœur 0, priorité 1, pile interne 8 Kio ; résultat publié par UI après fin. Annulation sans attente de worker dans LVGL. | Annulation avant entrée worker et pendant scan, nouvelle requête, erreurs EIO/OOM/admission, publication et catalogue précédent conservés. Le secours incrémental reste disponible si la tâche ne peut pas être admise. |
| Le bandeau audio copiait un snapshot complet toutes les250 ms et l’horloge en prenait un autre pour deux booléens réseau. | Observateur sur révision audio ; acquisition immédiate et événement conservé en cas de contention/OOM. Vue Wi-Fi scalaire. Horloge, callbacks réseau et bandeau ne redessinent que sur changement utile. | Vraies méthodes producteur/lecteur et intégration principale. Modèle audio 60 s : 1 440 copies de String après initialisation →0 ; producteur inchangé. |
| Les compteurs de profilage désactivés réservaient néanmoins578 o de symboles BSS internes. | État optionnel créé en PSRAM à la première activation, sans repli SRAM ; erreur explicite et interrupteur cohérent si allocation refusée. | Tests complets du module, refus d’allocation, générations, reset et chaîne UTF-8 ;216 o RTC conservés. Le coût global du lien est mesuré séparément. |
| Le renouvellement du catalogue pouvait déplacer la zone visible après restauration du focus. | Restauration du viewport après le focus passif, sans supprimer la navigation clavier. | Test réel LVGL de défilement et retour de menu. |

Les tables catalogue et staging étaient déjà en PSRAM. La version actuelle évite le staging inutile tant que seule la page native est utilisée : dans ce scénario hôte, leur allocation suivie passe de 49 152 à 24 576 o. Ce n’est pas une mesure du tas après démarrage sur matériel. Les 72 objets natifs du scénario sont conservés ; les applications ne sont pas retirées pour obtenir le chiffre.

## Purge de code ciblée

- Exclusions nominatives **Quake.app.elf**, **Duke3D.app.elf**, **AtelierImage.app.elf** retirées du catalogue, du chargeur concerné et du pipeline Factory courant. Les ELF se découvrent selon leur type/racine, sans règle ad hoc fondée sur leur ancien nom. Les préférences explicitement masquées par l’utilisateur restent effectives.
- Redirections imposées du fichier `AtelierImage.app.elf` vers Dessin supprimées dans trois routes : un ELF sélectionné passe par le chargeur normal.
- Ancien constructeur de carte d’accueil, son handler, table d’icônes, champ `symbol` inutilisé et paramètre booléen sans fonction retirés. Le constructeur actuellement utilisé conserve les applications et leur présentation.
- Sous-titre explicatif long d’« Ouvrir avec » retiré, les informations utiles restant accessibles dans l’aide. Commentaire devenu faux sur les trames ignorées actualisé ; aucun mécanisme de perte d’image ajouté.
- Les outils actifs `compile_linux.py`, `target_compile.py` et `main_target.py` découvrent le sketch courant. Les outils de préparation/validation/publication utilisent un contexte de livraison commun. Les règles de retrait de jeux ne sont plus reproduites par le générateur ou le validateur courant des ressources.

Ce travail ne prétend pas que tout code ancien est inutile ou a été supprimé. Les migrations de formats/données, la compatibilité NVS, les racines privées Python, les anciennes empreintes de ressources officielles, les backports SDK, les notices de licence et les preuves historiques ont un rôle réel. Les retirer par âge pourrait casser une carte existante, écraser une personnalisation ou supprimer une capacité. Les scripts historiques restent des références versionnées, pas les commandes actives de livraison.

## Événements, cœurs et mémoire

Une boucle FreeRTOS/LVGL peut distribuer des événements sans reconstruire l’écran. La version actuelle conserve les boucles nécessaires aux pilotes et timers mais supprime les travaux coûteux sans changement d’état. Les bibliothèques ouvertes gardent une vérification périodique de secours : certaines écritures POSIX ne publient pas de révision. Présenter l’ensemble du système comme intégralement déclenché par interruptions serait inexact.

Le P4 possède deux cœurs HP partageant ses régions mémoire. L’UI reste sur le cœur 1 configuré ; le nouveau scan s’exécute sur le cœur 0. Audio, vidéo, Python et plusieurs services utilisaient déjà le cœur 0 : il n’était pas ignoré, mais l’énumération de l’accueil était encore sur UI. La pile 8 Kio du scan est une dépense interne **transitoire**, pas un gain permanent de SRAM ; ses callbacks ne font pas de LVGL depuis worker. Les priorités plus fortes des services audio existants sont conservées. La configuration compilée est 360 MHz, distincte du maximum matériel 400 MHz.

Le C6 est une puce de communication avec sa propre mémoire et son firmware ESP-hosted. Ce n’est ni un deuxième tas partagé avec le P4, ni un cœur FreeRTOS supplémentaire pour les pointeurs et tâches du firmware P4. Lui déléguer du calcul général nécessiterait un protocole et un firmware propres ; ce changement n’est pas imposé à une communication actuellement fonctionnelle. Le cœur LP et sa SRAM ne remplacent pas non plus une pile HP ou une allocation DMA.

HP SRAM, SPM/TCM, LP/RTC, IRAM/DRAM et PSRAM restent distinctes. Les allocations ordinaires du projet privilégiaient déjà la PSRAM ; les piles/pilotes explicitement internes gardent leurs contraintes. Le SDK réserve notamment 256 Kio de SRAM HP au cache L2 : les768 Kio physiques ne représentent pas768 Kio de tas disponible. Les minima, plus grands blocs et piles réels se relèvent à la demande par `m` et `tasks`, sans diagnostic permanent de tas.

## Flashs au repos : ce qui reste à confirmer

Aucun nouveau défaut démontré n’a été trouvé dans la barrière de présentation, la copie cache ou la chaîne ISR au cours de ce lot. Les tests LVGL réels maintiennent les invalidations pendant l’attente DMA ; le modèle d’affichage inactif réalise0 flush sur 180 s. Le scénario Home réalise aussi0 flush pendant son repos, **avant comme après**, malgré les scans inutiles de l’ancienne version. Cela établit une activité d’E/S inutile ; cela ne démontre pas que les flashs turquoise viennent d’un redessin périodique.

Le MIPI-DSI balaie continuellement son framebuffer même si LVGL ne publie aucune nouvelle zone. Alimentation, sous-alimentation des données DSI, cache/DMA, concurrence PSRAM, révision de dalle ou interactions I²C restent à corréler sur matériel. Aucun reset périodique, délai de masquage, effacement d’image ou suppression de fonctionnalité n’a été ajouté.

La barrière conservatrice de deux événements DSI est maintenue. Elle peut limiter la cadence visible à environ 30 présentations/s dans un modèle 60 Hz ; ce n’est pas un taux mesuré sur Tab5. La réduire sans prouver la libération du framebuffer risquerait d’écrire dans une image encore balayée. L’absence de wait UI n’est pas une garantie de cadence physique maximale.

## Résiduels et validation

| Criticité | Point restant | Vérification utile |
|---|---|---|
| Haute | Origine des flashs turquoise et fluidité réelle | Repos puis souris, compteurs DSI/redessins et journal de latence, révision écran, alimentation, charge PSRAM. |
| Haute | Manque de mémoire Bluetooth ou fragmentation sous charges lourdes | Tas par capacité, plus grand bloc, besoin exact et piles avant/après 20 cycles ; la PSRAM libre ne suffit pas à un besoin interne. |
| Haute | Mutations concurrentes du même chemin, coupure physique pendant FAT, code ELF non coopératif | Copies de test et journaux ; aucune isolation matérielle ELF ni durabilité universelle FAT annoncée. |
| Moyenne | Ouvertures/cartes/catalogues de médias, certaines sauvegardes et actions coffre encore synchrones | Profilage par phase et taille réelle des données ; les déplacer exige un protocole de propriété/annulation sûr. |
| Moyenne | Débordement USB susceptible de perdre des événements | Reproduire sous charges lourdes ; l’ordre des transitions doit être préservé avant de changer la politique du producteur. |

La compilation complète, les sources figées, l’image, les partitions et les empreintes sont contrôlées dans [VERIFICATION.md](VERIFICATION.md). Les mesures avant/après se trouvent dans [BILAN_RESSOURCES_0.8.43.md](BILAN_RESSOURCES_0.8.43.md). Les scénarios sur appareil sont dans [TESTS_TAB5_0.8.43.md](TESTS_TAB5_0.8.43.md). Les audits de détail, commandes et modèles sont joints dans `tools/audit-0843` et `tools/verification`.
