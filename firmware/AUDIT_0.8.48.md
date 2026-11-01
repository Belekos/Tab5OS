# Tab5OS 0.8.48 — audit et corrections ciblées

## Référence et niveau de preuve

Cette version répond aux flashs encore observés en 0.8.47, aux claquements au lancement musical, à l'échec d'un deuxième stream vidéo et au coût du premier trait dans Dessin. Ces symptômes n'étaient donc pas résolus sur l'appareil par la livraison précédente. Aucun essai physique de la 0.8.48 n'a été effectué ici.

Référence récupérée : `Tab5OS_0.8.47_Refonte_Stabilisation.zip`, 171 065 019 octets, SHA-256 `77c881f040ffbdc07815f7ab954acbc5107066a8a24fa48701758f088308dec7`. Le CRC de ses 6 925 entrées a été vérifié avant extraction. Les fichiers temporaires du travail interrompu avaient disparu ; les corrections ont été reconstruites sur cette archive et les tests réexécutés. Les anciens résultats ne sont pas présentés comme des exécutions nouvelles.

Les preuves détaillées figurent dans `tools/verification/*0848*.json`, les essais reproductibles dans `tools/*0848/` et les revues par module dans `tools/audit-0848/`. Les tests hôte utilisent le code de production, avec les frontières matérielles adaptées ; ils ne mesurent ni la bande passante réelle de la PSRAM, ni le son, ni les temps de réponse de l'écran physique.

## Affichage : défaut fondamental confirmé

La 0.8.47 plaçait du code DSI/DW-GDMA en IRAM, mais les bibliothèques précompilées ne demandaient pas les interruptions IRAM correspondantes. La localisation du code n'empêche pas le SDK de masquer une interruption pendant une opération Flash. De plus, les structures DW-GDMA allouées avec les capacités par défaut pouvaient résider en PSRAM : Tab5OS appelle `heap_caps_malloc_extmem_enable(0)` avant l'initialisation du matériel.

Le pilote DSI redémarre son transfert à chaque fin d'image via DW-GDMA. Il ne suffit donc pas de suspendre LVGL ou de placer l'écriture NVS sur le deuxième cœur. L'écran continue de lire son framebuffer lorsque l'utilisateur ne fait rien ; ces lectures et la progression des interruptions restent nécessaires.

Correction : reconstruire les deux unités natives de la version exacte du SDK avec leurs options de sécurité cache, conserver les contextes nécessaires en mémoire interne, vérifier le code et les données utilisés pendant ces interruptions. Les bibliothèques SDK installées ne sont pas modifiées globalement. La compilation du projet lie explicitement les deux unités corrigées et vérifie le résultat final. Un simple `#define` ajouté au sketch n'aurait pas corrigé le code déjà compilé du SDK.

Le message d'underrun du pilote ne fait plus de sortie série depuis l'interruption. Un hook C obligatoire, en IRAM, incrémente des compteurs internes ; la tâche UI extrait les nouveaux événements et les journalise, au plus une fois par seconde. Le premier événement est immédiat. L'agrégation ne retranche pas les erreurs du compteur total. Aucune temporisation de transfert, réinitialisation de dalle ou tâche de diagnostic supplémentaire n'est ajoutée.

**Limite :** cette correction établit et traite une rupture possible du fonctionnement pendant les accès Flash. Elle ne prouve pas que tous les flashs observés ont cette origine. Une sous-alimentation DSI par contention PSRAM, un problème de cache, d'alimentation ou de révision de dalle nécessite encore une corrélation sur la Tab5. L'absence d'un compteur d'erreur ne prouve pas à elle seule l'absence d'un défaut visible.

## Affichage : moins de copies lors des changements de page

Le présentateur copiait les zones sales de l'image précédente vers l'image libre avant de savoir si LVGL allait les remplacer entièrement. Un changement de menu pouvait donc recopier une grande image immédiatement avant de l'écraser.

Le miroir est désormais différé jusqu'à la connaissance des zones nouvellement écrites. Les parties recouvertes sont soustraites ; seules les parties encore nécessaires sont copiées avant la publication. Le stockage des intervalles est borné, optionnel et en PSRAM. En cas d'échec de cette petite allocation, le chemin précédent reste disponible. Les barrières de fin de transfert et les erreurs de cache/publication demeurent prises en compte.

Un framebuffer RGB565 de 720 × 1 280 occupe 1 843 200 octets. Le déplacer intégralement en SRAM n'est pas réaliste. Un petit tampon interne ne remplace pas le framebuffer que le DMA DSI lit continuellement. Le transfert natif utilise déjà une petite zone de travail interne ; ajouter une copie intermédiaire sans modifier le contrat du pilote risquerait d'accroître le trafic. La correction réduit donc d'abord le trafic démontré inutile. Aucun changement arbitraire d'horloge d'écran n'est livré.

Le contrat des écritures natives demeure : annoncer les rectangles avant d'écrire. La protection des chemins livrés n'est pas une isolation matérielle contre un ELF externe qui écrirait à une adresse arbitraire.

## Audio : résidu du morceau précédent confirmé

Dans M5Unified, l'arrêt libérait les demandes mais conservait l'état d'interpolation des canaux. Une nouvelle lecture pouvait réutiliser des échantillons du morceau précédent. Le test enchaîne un signal, un arrêt complet et un nouveau flux composé uniquement de zéros ; il reproduit cette contamination avec le code précédent.

La correction remet à zéro les seuls champs d'interpolation après la fin effective du worker. Le volume et les paramètres du canal restent conservés. La pause et la lecture continue ne passent pas par cette remise à zéro. Aucun fondu, silence artificiel ajouté au fichier ou suppression des premières notes ne masque le problème.

**Limite :** un claquement dès la première lecture après démarrage peut avoir une autre origine analogique ou I²S. La séquence de préparation du codec existante reste à qualifier acoustiquement ; la correction de l'état numérique ne certifie pas l'absence de tout transitoire analogique.

## Vidéo : allocations, entrées JPEG et succession des streams

Le probe `jpeg_decoder_get_info` du SDK alloue des tables pour chaque image et fuit sur certains formats de sous-échantillonnage non pris en charge. Il ne faut pas assimiler systématiquement cette fuite à de la SRAM : avec la politique de Tab5OS, les allocations par défaut privilégient la PSRAM et peuvent se rabattre sur la mémoire interne selon la pression.

Le nouveau probe est borné par la longueur du flux, sans allocation, respecte les longueurs des segments et ignore les miniatures éventuellement contenues dans APP/EXIF. Il contrôle aussi les indices et longueurs de tables pouvant mettre en défaut le parseur natif. Le décodage matériel natif reste utilisé. Les tables par défaut des MJPEG compatibles ne sont pas supprimées par une exigence artificielle de tables explicites. Ce garde ne prétend pas être un validateur exhaustif de JPEG ni une frontière de sécurité isolant le décodeur.

Les buffers de sortie n'ont plus besoin d'être mis intégralement à zéro juste avant que le décodeur les remplisse. Ils ne doivent être exposés qu'après un décodage réussi ; alignement, capacités et propriété des buffers restent contrôlés. Les réallocations d'entrée ne recopient que les octets encore nécessaires.

Pour un remplacement déjà en attente, le worker vidéo termine la libération des ressources du flux courant puis prend le dernier flux demandé dans la même tâche. Cela évite de réclamer une nouvelle pile de 24 Kio alors que FreeRTOS n'a pas encore rendu celle de la tâche terminée. Sans nouvelle demande, le worker se termine normalement. Une arrivée après sa publication finale suit le chemin de démarrage existant : cette fenêtre et une véritable fragmentation peuvent encore entraîner un refus mémoire. Le verrou, l'annulation et les pointeurs d'image demeurent déterminants ; aucune libération forcée d'une autre tâche n'est utilisée.

## Dessin : coût du premier trait

Au début d'un geste, l'ancien code copiait tout le calque actif pour l'historique. La réservation mémoire nécessaire à l'annulation est conservée avant toute modification, mais les pixels sont maintenant capturés par blocs de 256 pixels seulement lorsqu'ils vont être touchés. Le geste n'effectue pas d'allocation supplémentaire. Undo, redo et annulation échangent uniquement ces blocs.

Cela réduit les écritures au premier clic, pas la taille maximale de mémoire réservée à l'historique. Un masque de sélection complexe peut encore devoir être copié ; une opération couvrant toute la toile reste coûteuse. Cette distinction évite de présenter une baisse de copies comme une baisse équivalente de mémoire allouée.

## Activité au repos et deux cœurs

La tâche audio était recréée toutes les cinq secondes dès que le Wi-Fi était activé, notamment pour actualiser le signal. Elle est désormais démarrée pour une vraie commande, une transition nécessaire du mode d'économie Wi-Fi ou une demande explicite de mesure. Une mesure de signal est effectuée à la connexion et sur demande de l'interface, avec la limitation de fréquence existante. Les commandes essentielles et les mécanismes d'admission mémoire gardent la priorité.

Le deuxième cœur du P4 est déjà utilisé par le décodage et plusieurs services. Il ne constitue pas une seconde PSRAM, ni un moyen de contourner une opération Flash suspendant les caches. Les traitements longs restent déportés lorsque leur propriété et leurs ressources le permettent ; LVGL conserve un propriétaire. Le C6 reste le coprocesseur de communications, pas un processeur général auquel envoyer arbitrairement le rendu ou le décodage.

## Conservation et problèmes résiduels

Les tables de partitions, formats des documents, secrets, clés et politiques d'autosauvegarde ne sont pas changés. Les préférences Média conservent leur format et leur persistance existants ; retirer les écritures pour cacher un flash n'aurait pas été une correction. Les tests de persistance et de coexistence réseau ont été réexécutés. Les anciens tests conservés dans le projet sont un historique, pas des éléments compilés dans le firmware. Le contrôle de conservation retrouve 1 900 sources inchangées et 15 sources modifiées ou ajoutées avec preuves associées. Le catalogue ELF ne contient pas de liste d’exclusion par nom. Des copies temporaires de journaux et d’index ont en revanche été trouvées dans les anciennes archives : un filtre ciblé les exclut de la distribution, tout en conservant les journaux authentiques et leurs empreintes. Il retire 90 383 350 octets décompressés supplémentaires sans supprimer de fichier utilisateur.

Priorités matérielles restantes : flash à l'action et au repos (haute), pop à froid (haute), deuxième stream après libération tardive ou fragmentation (haute), marge des piles et concurrence radio/Cartes/SD (haute). Le protocole `TESTS_TAB5_0.8.48.md` précise les observations nécessaires. Cette livraison ne constitue pas une qualification de production.

Références primaires : ESP-IDF 5.5.4 et M5Unified 0.2.23 utilisés par la compilation ; [allocation mémoire ESP32-P4](https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32p4/api-reference/system/mem_alloc.html) ; [notes Espressif LCD/MIPI-DSI](https://docs.espressif.com/projects/esp-faq/en/latest/software-framework/peripherals/lcd.html). Les mesures avant/après retenues sont dans `BILAN_RESSOURCES_0.8.48.md`.
