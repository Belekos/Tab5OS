# Tab5OS 0.8.47 — diagnostic et refonte vérifiable

## Portée et référence

Travail effectué sur la livraison 0.8.46 authentifiée, sans repartir d'un ancien projet supposé. Votre retour est conservé comme preuve matérielle : les flashs et claquements persistent, tandis que les pertes SD dans Cartes ne sont plus rapportées. Les anciennes validations hôte n'ont donc pas résolu l'acceptation matérielle.

La revue couvre l'architecture commune, les chemins de rendu, les listes Média, le codec/I²S, l'admission et la récupération mémoire, les tâches et la persistance. Un inventaire reproductible couvre les attentes déclarées dans les sources du firmware et de ses dépendances embarquées, y compris des branches conditionnelles non compilées ; il ne constitue pas une revue manuelle exhaustive de chaque branche. Les corrections concrètes sont regroupées suivant `ARCHITECTURE_0.8.47.md`.

## Causes établies et corrections

| Module | Défaut établi | Correction et limite |
|---|---|---|
| Média/LVGL | Chaque reconstruction masquait/réinitialisait les lignes, réappliquait leurs styles et options, même sans changement. | Modèle préparé séparément, lignes conservées, publication unique et comparaisons des propriétés. Les vrais changements continuent à redessiner la zone utile. |
| Média/navigation | LVGL décochait un onglet CHECKABLE avant le callback, alors que le callback ignorait un clic sur l'onglet déjà actif. | État exclusif piloté par la vue réellement validée. |
| Média/tri | Une allocation PSRAM par nom, plus la table de tri. | Noms regroupés dans une arène ; mêmes critères, limites et accès aux métadonnées. Les erreurs restent explicites. |
| Média/propriété | Une liste conservée après échec peut porter des indices périmés si le catalogue source vient d'être rechargé. | Générations de catalogues et validation des actions/commandes. Une ancienne référence ne doit jamais lancer ou supprimer un autre élément. Ce risque découvert en relecture a été traité avant livraison. |
| Préférences | Écritures NVS du navigateur et volume depuis la tâche UI, avec un blob navigateur de 3 444 octets. | Instantané immuable PSRAM et un worker temporaire à pile interne. Délais de regroupement, reprises et format existants conservés. Aucun document sauvegardé par ce mécanisme. |
| Audio/M5Unified | Callback activant le codec avant la configuration et le démarrage des horloges I²S ; démutage pendant la séquence de configuration. | Horloges et parcours DMA de silence avant activation, mute pendant configuration, contrôle des erreurs. La taille de DMA provient de l'API native, pas d'une supposition sur les slots physiques. |
| Audio/erreurs | Certaines erreurs de sortie étaient assimilées à de la contre-pression ou à une fin de fichier, avec risque de réessayer indéfiniment. | Échec terminal de la session, remonté au propriétaire ; reprise seulement par une nouvelle demande. Les consommateurs audio sont vérifiés ensemble. |
| Affichage | Conversion affine générique M5GFX pour chaque bande RGB565, avec accès PSRAM dispersés. | Copie spécialisée par blocs 16 × 16 vers l'image arrière, rotations/stride/ordre des octets vérifiés. Ancienne voie conservée en repli. Aucune promesse de FPS sans mesure matérielle. |
| Admission mémoire | Verrou à sondage par tick sans héritage de priorité ; workers réveillés chaque tick pendant une collecte UI. | Mutex FreeRTOS et événement de fin par demande ; mêmes marges, timeout et propriétaire des destructions. |
| Écriture cachée | Indexation structurelle poursuivie à chaque service UI alors que le document est caché. | Suspension tant que la vue est cachée, reprise de la progression existante. Seuls les checkpoints syntaxiques à reconstruction tolérant l'OOM sont libérés sous pression. |
| Nettoyage | Cinq anciens callbacks Média sans appelant subsistaient dans le fichier principal. | Définitions retirées après recherche des usages ; les helpers encore partagés avec Cartes/Wi-Fi sont conservés. Aucun gain binaire fictif revendiqué pour du code déjà éliminé par le linker. |

## Relectures et fausses pistes éliminées

La séquence de silence audio a été confrontée à `i2s_channel_get_info()` et au pilote IDF réel : nombre de slots physiques et taille réellement allouée ne sont pas interchangeables. Le chemin sans PCM, notamment pendant HTTP, a été revu : le codec externe garde ses horloges et le worker attend une notification ; le stop au repos du DAC interne ne s'applique pas à la Tab5.

L'ancien deuxième essai Speaker réappliquait la géométrie 256 × 8 déjà utilisée par le firmware livré. Son retrait ne représente donc pas une suppression d'une récupération par réduction de DMA dans cette configuration. Les options expérimentales non activées ne sont pas décrites comme testées sur matériel.

Les fonctions de callbacks LCD/DW-GDMA sont placées en IRAM par les règles du linker SDK, même sans attribut dans certaines définitions C. L'ELF final est contrôlé ; l'absence d'attribut seul n'est pas retenue comme cause de flash.

Le miroir entre les deux framebuffers et les barrières DMA sont conservés. Leur suppression demanderait un suivi exact des zones écrasées, sans bénéfice suffisamment établi pour ajouter ce risque. Les réglages de dalle, partitions, version du SDK et protections SD ne sont pas remplacés arbitrairement.

La relecture du catalogue d'applications ne retrouve pas de liste d'exclusion nominative d'anciens ELF. Il parcourt `/Applications`, `/Apps`, `/MicroPython` et leurs équivalents USB montés ; l'extension `.elf` est reconnue sans distinction de casse, avec des conventions d'entrée de dossier `main.app.elf` et `main.elf`. Les fichiers commençant par un point restent cachés ; l'épinglage relève des préférences utilisateur, conservées. Les limites existantes de 64 entrées de catalogue et de 384 entrées examinées peuvent encore tronquer un scan. Ce constat n'est ni une nouvelle correction du chargeur, ni une garantie d'afficher tout ELF quelle que soit son organisation.

## Cœurs, mémoire et attentes

Le second cœur est déjà utilisé. Les tâches décodage/haut-parleur/vidéo sont prioritaires sur le cœur 0 ; l'UI et une partie du réseau/Cartes utilisent le cœur 1. Le nouveau worker de préférences n'impose pas d'affinité et reste de priorité 1. Les événements FreeRTOS remplacent des sondages là où une vraie fin de travail est disponible.

La PSRAM n'est pas interchangeable avec la SRAM interne : une pile intervenant lorsque le cache externe est désactivé, notamment pour NVS, doit rester interne. Les contraintes des autres piles et buffers dépendent des API et périphériques utilisés. Les 768 Kio physiques HP ne sont pas un heap de 768 Kio : cache L2, code, sections et réserves occupent des régions distinctes. TCM/SPM désignent ici la même banque spécialisée ; LP SRAM est séparée. Les capacités DMA recoupent des heaps existants et ne doivent pas être ajoutées comme une mémoire indépendante.

Le SDK verrouillé suspend les deux cœurs et les accès cache pendant certaines transactions Flash. Une tâche NVS séparée retire l'attente applicative de l'UI mais ne supprime pas ce mécanisme matériel/SDK. XIP nécessite un SDK et un démarrage reconstruits, plusieurs Mio de PSRAM et une qualification OTA/Hosted/affichage ; aucun simple `#define` trompeur n'est ajouté.

Les inventaires JSON/CSV sous `tools/audit-0847` donnent les appels d'attente, leurs arguments et leur contexte. Les nombres sont lexicaux, incluent des branches conditionnelles et ne mesurent pas le temps CPU. Les waits utiles au DMA, à une file, à un événement, au watchdog ou au protocole restent conservés. L'attente courte de restitution différée d'une pile n'a pas été remplacée par une destruction forcée.

## Problèmes restant ouverts

| Criticité | Point | Prochaine preuve nécessaire |
|---|---|---|
| Haute | Flashs partout, particulièrement Média/Dessin, et au repos | Essais Tab5 avec compteurs LVGL/publications/underruns/intervalle DSI et corrélation des jobs NVS. Aucune cause unique physique n'est affirmée. |
| Haute | Claquement initial ou reset audio/vidéo | Essai acoustique et journal de démarrage/erreur après correction de la séquence et des boucles de reprise. |
| Haute | Épuisement réel de mémoire après usage, BLE/Mesh, pile insuffisante | Relevés `m` à états identiques, bloc maximal et marges de piles ; les tests hôte ne mesurent pas les heaps matériels. |
| Haute sous OOM | Certains conteneurs C++ de l'éditeur emploient encore un allocateur qui arrête le programme sans exceptions en cas d'échec | Rendre ces opérations récupérables avec un contrat d'erreur cohérent. Leur cache structurel n'est pas évincé par cette version pour ne pas multiplier ce risque au retour. |
| Moyenne | Premiers scans de catalogue/dossiers, tri SD taille/date, certaines images et transformations/sauvegardes synchrones | Exécuter l'opération entière dans un contexte adapté avec annulation et propriété explicites, plutôt que déplacer seulement une étape. |
| Moyenne | Logs UART synchrones, polling série, entretien audio et quelques appels Wi-Fi répétés | Profiler puis remplacer sans perdre les diagnostics, les réceptions série ou les transitions d'économie d'énergie. |
| Haute si reproduite | Attentes de terminaison de pilotes NimBLE/USB/DMA et récupération Python | Journaux pilotes ; ne pas libérer un buffer ou une tâche encore utilisé. |
| Limite structurelle | ELF natifs sans isolation matérielle | Un pointeur invalide peut encore corrompre l'OS. Le chargement ELF n'est pas une sandbox. |

## Validation et données

`BILAN_RESSOURCES_0.8.47.md` et `VERIFICATION.md` distinguent les mesures avant/après, compilation et tests réellement exécutés. Les scripts, frontières matérielles adaptées et empreintes des sources sont joints. Les tests matériels nécessaires sont dans `TESTS_TAB5_0.8.47.md`.

Les changements ne suppriment pas les documents non enregistrés et ne modifient pas les choix d'autosauvegarde, les formats des préférences ou les clés utilisateur. Les tests Writer vérifient la conservation du texte, de l'historique et de la sélection pendant la récupération des checkpoints. Aucun formatage, effacement de partition ni reflashage d'appareil n'est réalisé par ce travail. La conservation complète après mise à jour doit encore être vérifiée sur l'appareil ; cette version n'est pas déclarée qualifiée pour production avant ces essais.
