# Affichage — correction du travail mémoire redondant, 0.8.46

Le retour de la tablette prime sur les résultats hôte : les flashs Dessin/Média et les rares flashs au repos restent des symptômes non résolus matériellement après 0.8.45. Cette correction ne les déclare pas supprimés. Elle enlève un coût établi du chemin d'affichage, avec comparaison sur la version réellement livrée.

## Chemin réel réexaminé

LVGL 9.6 produit des rectangles dans sa mémoire de travail. `displayFlush` ouvre une seule transaction M5GFX par trame LVGL. Le présentateur attend sans bloquer que son ancien tampon ne soit plus balayé, reporte les changements précédents dans le second tampon, puis M5GFX écrit les nouveaux rectangles. `Panel_FrameBufferBase::display`, appelé à la fin de `endWrite`, synchronise le cache via `Cache_WriteBack_Addr` ; il **ne publie pas** un autre tampon à l'insu du présentateur. L'appel `esp_lcd_panel_draw_bitmap` n'a lieu qu'ensuite dans `Tab5DisplayPresentEndFrame`.

Le pilote exact IDF 5.5.4, commit `735507283d`, reconnaît ses propres framebuffers. Son appel de dessin sélectionne l'index sans recopier l'image. La soumission Tab5OS de la première ligne ajoute une synchronisation de 1 440 octets. Le callback DMA réarme le descripteur avant le callback `on_refresh_done` dans le SDK ESP32-P4 utilisé. La barrière conservatrice de deux événements est conservée. Les deux images et les pixels en cours de balayage restent soumis au même protocole de propriété.

## Anomalie confirmée

La 0.8.45 synchronisait immédiatement les pixels recopiés de la trame précédente, puis synchronisait à nouveau les pixels dessinés dans la nouvelle trame. Quand les zones se recouvrent — notamment les changements de page et les mises à jour répétées d'une grande image — une même zone de cache est parcourue deux fois. Il ne s'agit pas d'une hypothèse de panne : les appels réels des deux versions montrent la duplication.

Cela ajoute du travail sur la mémoire partagée avec le balayage continu de l'écran. Le fait que cette duplication soit l'origine du flash turquoise sur la tablette n'est **pas établi**. Les évictions de cache, le débit physique PSRAM, les interruptions et les autres maîtres DMA ne sont pas mesurés par le test hôte.

## Correction ciblée

Les deux tables de lignes déjà allouées servent désormais à deux durées de vie distinctes :

- Le présentateur conserve les rectangles dessinés dans la dernière trame, nécessaires pour reconstruire le prochain tampon arrière.
- Le cache rassemble les zones recopiées et les rectangles de la nouvelle trame, afin de synchroniser leur union après le rendu et avant publication.

Un callback synchrone transmet seulement les coordonnées physiques des lignes recopiées. Il ne conserve ni adresse d'objet temporaire ni propriété d'un buffer. Le miroir CPU et les pixels LVGL restent dans le tampon arrière. Le cache n'est différé que dans la transaction vérifiée de ce présentateur à deux images. Les appels génériques, le manque de métadonnées et le présentateur indisponible conservent le chemin immédiat antérieur. Une écriture imprévue dans la transaction déclenche la synchronisation complète du tampon, miroir compris.

Après une erreur de cache, de sélection ou de publication, le prochain essai reconstruit entièrement le tampon arrière à partir du tampon visible avant de dessiner. Une erreur ne publie pas une image partiellement synchronisée. La reprise reste celle déjà existante, sans nouveau redémarrage ni attente arbitraire.

Aucune troisième image, tâche, allocation par trame ou minuterie n'est ajoutée. Les deux images DSI existantes occupent 3 686 400 octets de PSRAM ; les deux tables de 1 280 lignes occupent toujours 10 240 octets au total. Un état booléen privé permet de reconnaître la transaction fusionnée. Les timings de dalle, priorités AXI, fréquence CPU/PSRAM, orientation, caches SDK, broche LCD_RST et barrière DMA sont inchangés.

## Comparaison exécutée

Référence extraite du ZIP livré 0.8.45, empreinte `46fd5da89ebb693a61d3ab34e2d3f3480a714aff76c095ce339f290ee8641573`. Les mêmes scénarios et mêmes corps exacts du pilote sont compilés avec les sources de chaque version.

| Série hôte | 0.8.45 | 0.8.46 |
| --- | ---: | ---: |
| 40 mises à jour successives de toute l'image : octets passés aux appels de synchronisation cache | 145 843 200 | 73 785 600 |
| Même série : nombre d'appels cache | 8 978 | 4 560 |
| Publications sur la suite complète | 545 | 545 |
| Publication ajoutée pendant 1 000 événements DMA sans rendu | 0 | 0 |
| Écriture dans l'image balayée sur les chemins à deux tampons contrôlés | 0 | 0 |

La diminution de 49,4 % concerne les **plages d'adresses parcourues par les appels cache** de cette série. Ce n'est pas une réduction mesurée de 49,4 % du trafic physique PSRAM, de la latence, du CPU ou des flashs. Le miroir CPU lui-même reste nécessaire et n'est pas supprimé.

La suite couvre 500 trames partielles avec conservation des pixels non redessinés, 64 combinaisons rotation/offset et 12 800 rectangles, deux intercalages d'interruption dans l'appel réel du pilote, des échecs cache et publication suivis de reprise. Des essais distincts confirment que l'échec d'allocation de la table cache et l'indisponibilité du second tampon ne bloquent pas le chemin de repli antérieur. ASan/UBSan sont actifs ; LSan est indisponible dans cet environnement.

Le test d'intégration avec **LVGL 9.6.0 réel** couvre 97 trames, 96 publications, 201 450 ms simulées, un blocage temporaire des événements DMA, une panne de cache, une pause de traitement de 2 750 ms et 180 secondes d'écran stable avec mises à jour de widgets cachés. Aucun nouveau transfert n'est créé au repos. La propriété du tampon balayé est contrôlée séparément de l'état du cache CPU et de la mémoire externe simulée.

Les cinq unités P4 `Present`, `Cache`, `Refresh`, `Signals`, `Bandwidth` ont été compilées avec le compilateur et les dépendances réelles de la release. La compilation complète et les tailles finales sont établies par le rapport général de compilation, pas par ces tests objet.

Preuves : `tools/verification/display-driver-0846.json`, `display-0846.json`, `display-target-0846.json`. Sources des tests : `tools/display-driver-tests-0846`, `tools/display-tests-0846`. Les preuves des anciennes versions restent conservées.

## Attentes et délais de ce sous-système

| Élément | Nature et décision |
| --- | --- |
| `Present` 100 ms | Seuil de diagnostic de la barrière ; aucun sommeil ni boucle d'attente active. Conservé. |
| `Present` 1 000 ms, `CacheRecovery` 1 000 ms | Reprise seulement après erreur constatée ; aucun rafraîchissement périodique en état sain. Conservée. |
| `DisplayRefresh` | Suspend seulement le timer de rendu LVGL tant que le tampon n'est pas disponible ; les autres services UI continuent. |
| Appels cache | Toujours bornés à 16 Kio ; le SDK peut masquer les interruptions pendant chaque appel. La fusion réduit les appels redondants, sans inventer une garantie de temps maximal. |
| Délais de `Panel_DSI::init_panel/init` M5GFX | Réinitialisation/commandes d'initialisation au démarrage, prescrites par la séquence de dalle ; pas de délai périodique à supprimer. |

Aucun timer de 90 secondes ni remise à zéro périodique de la dalle n'a été trouvé dans ce chemin. Une trame DSI continue toutefois d'être balayée au repos : l'absence de rendu LVGL ne signifie pas l'absence d'accès mémoire de l'écran.

## Points matériels restant ouverts

Le SDK signale explicitement qu'un underrun du pont DSI peut donner une image bleue lorsque la mémoire n'alimente plus le DMA à temps. Ce mécanisme demeure plausible, sans preuve que le compteur augmente sur cet appareil. La variante de dalle et l'activité de présentation doivent être enregistrées au démarrage ; `double buffering: active` est une condition d'application de la fusion. Le repli reste fonctionnel mais n'offre pas les mêmes propriétés d'absence de déchirement.

Reproduire un clic Dessin, un changement d'onglet Média et une période au repos ; prendre un relevé série `d` avant/après et garder le journal de démarrage. Une augmentation de `underruns` avec `observed=1` établit une alimentation DSI insuffisante, pas à elle seule la source de contention. Un compteur inactif ne prouve pas l'absence d'underrun. Les mesures de pile, heap et tâches du rapport mémoire complètent cette observation, sans remplacer un essai de la dalle.
