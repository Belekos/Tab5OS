# Affichage — réexamen du pilote réel, 0.8.45

Les flashs corrélés aux clics dans Dessin et aux commandes Média restent un défaut matériellement non résolu. L'absence d'invalidation dans un ancien test d'accueil au repos ne permettait pas d'exonérer le chemin DSI lors d'un vrai transfert. Cet audit réexamine donc les routines du SDK effectivement fourni, en plus du code de présentation.

## Sources réellement confrontées

Le fichier `versions.txt` du SDK utilisé indique ESP-IDF **v5.5.4, commit 735507283d**. La base de compilation actuelle utilise `esp32p4_es-libs/3.3.9`, CPU 360 MHz, PSRAM HEX 200 MHz et révision minimale 0.1. La variante voisine `esp32p4-libs`, configurée pour révision 3 et 400 MHz, n'est pas la configuration de ce firmware.

La source C du pilote a cette fois été récupérée intégralement par le connecteur GitHub, à ce commit précis : [esp_lcd_panel_dpi.c](https://github.com/espressif/esp-idf/blob/735507283d/components/esp_lcd/dsi/esp_lcd_panel_dpi.c), blob `74c95b4a715c495654a40c0588684fd0dea9ab76`. Les routines et leur provenance sont conservées sous `tools/display-driver-tests-0845/upstream`, avec leurs licences. Le désassemblage de l'archive SDK liée confirme l'appel de sélection/réactivation du DMA avant le callback de rafraîchissement dans cette configuration.

M5GFX 0.2.30 et M5Unified 0.2.23 ont été relus dans les copies de dépendances réellement préparées. Les méthodes de rotation M5GFX utilisées par les tests sont copiées de ces sources, avec leur empreinte.

## Résultats établis et limites

| Point contrôlé | Résultat du code et des essais | Ce qui n'est pas établi |
| --- | --- | --- |
| Soumission d'un framebuffer IDF | Le pilote reconnaît un pointeur dans ses propres framebuffers, synchronise les lignes demandées et sélectionne leur index ; il ne copie pas les pixels dans ce chemin | Le débit réel disponible sur la tablette |
| Soumission Tab5OS 720 × 1 | Le passage SDK ajoute une synchronisation de 1 440 octets, pas de toute l'image ; les autres zones sont déjà synchronisées par le cache Tab5OS | L'absence d'underrun pendant les synchronisations et copies précédentes |
| Réutilisation des buffers | Dans le SDK compilé, le callback DMA sélectionne le descripteur de l'index demandé puis invoque `on_refresh_done`. La barrière existante de deux événements reste conservatrice | Une mesure de cadence, latence ou qualité d'affichage physique |
| Rotation et filtrage du cache | Les méthodes M5GFX exactes et le calcul des zones Tab5OS concordent pour 64 combinaisons rotation/offset et 12 800 rectangles | L'exécution de toutes les méthodes graphiques M5GFX sur une puce |
| Reset écran/I²C | La broche LCD_RST reste en entrée avec pull-up après initialisation ; l'amplificateur écrit un autre bit de sortie, sans cache logiciel de port | Un signal électrique effectivement mesuré sur LCD_RST |
| Miroir puis rendu | Un grand rectangle peut être recopié/synchronisé pour le miroir puis modifié/synchronisé à nouveau | Que ce trafic soit la cause exacte des flashs signalés |

La fonction native `dpi_panel_draw_bitmap` ignore le résultat de sa propre synchronisation cache dans ce chemin. Tab5OS effectue déjà ses synchronisations contrôlées avant la soumission ; une erreur de ces synchronisations empêche la publication. Aucun contournement du SDK n'est ajouté sans preuve d'une erreur réelle de son appel final.

## Hypothèse de bande passante à mesurer

Le pilote IDF exact journalise un underrun quand la mémoire externe ne fournit pas les données à temps et indique qu'il peut produire une image bleue. Cela rend ce mécanisme compatible avec le symptôme turquoise/bleu rapporté ; **cela ne prouve pas qu'il s'est produit sur cet appareil**. La [documentation officielle IDF 5.5.4](https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32p4/api-reference/peripherals/lcd/dsi_lcd.html) précise également le lien possible entre fréquence pixel, bande passante DMA et scintillement.

Les valeurs ci-dessous sont calculées à partir des réglages demandés par M5GFX, sans tenir compte d'un éventuel arrondi du diviseur ; ce ne sont pas des mesures :

| Configuration BSP | Fréquence pixel demandée | Pixels totaux par trame | Cadence théorique | Lecture RGB565 active moyenne | Débit pendant les pixels actifs |
| --- | ---: | ---: | ---: | ---: | ---: |
| ILI9881C | 80 MHz | 940 × 1 324 | 64,28 Hz | 118,48 Mo/s | 160 Mo/s |
| ST7121 | 70 MHz | 802 × 1 524 | 57,27 Hz | 105,56 Mo/s | 140 Mo/s |
| ST7123 | 80 MHz | 802 × 1 510 | 66,06 Hz | 121,76 Mo/s | 160 Mo/s |

Les copies CPU, captures d'annulation, décodages et synchronisations partagent cette mémoire avec le balayage continu. Le SDK ne configure pas ses opérations C2M par morceaux ; Tab5OS les borne déjà à 16 Kio. Chaque appel entre toutefois dans une section critique interne au SDK : dire que les interruptions restent toujours disponibles aurait été incorrect. La priorité AXI existante vise bien le port mémoire DW-GDMA 1, bits 16..19 ; ce n'est pas une erreur de registre constatée.

Une refonte pour fusionner les synchronisations du miroir et du rendu pourrait réduire un trafic redondant, mais modifierait le protocole de propriété et de récupération des buffers. Elle n'est pas introduite dans cette correction sans indication matérielle suffisante. Ni fréquence de dalle, ni priorité, ni délais, ni reset, ni nombre de buffers ne sont changés pour masquer le symptôme.

## I²C et variantes de matériel

La [documentation M5Stack Tab5](https://docs.m5stack.com/en/core/Tab5) prescrit LCD_RST en entrée avec pull-up à l'état haut pour les dalles ST, et cite M5Unified ≥ 0.2.23/M5GFX ≥ 0.2.30. Les dépendances présentes appliquent cette configuration, tant dans l'initialisation M5GFX que dans Power.begin. `PI4IOE5V6408_Class::digitalWrite` utilise directement le même read-modify-write que les appels de l'amplificateur ; aucun cache de sortie divergent n'a été trouvé. Les redémarrages audio peuvent ajouter des opérations I²C, mais leur transformation en reset LCD n'est pas démontrée par ce code.

## Correction de diagnostic uniquement

`tab5DisplayPresentDumpConfiguration` ajoute un relevé à la demande : fréquence pixel **demandée**, taille, rotation et porches réels de la configuration. Le service UI l'appelle via la commande série `d`. La fonction n'alloue pas de mémoire, n'ajoute aucune tâche ni timer, ne lit aucun registre périphérique et n'écrit ni configuration ni écran. Le garde vérifie d'abord la tâche propriétaire, le matériel Tab5 et le bus DSI avant le cast existant.

Ce relevé ne prétend pas identifier le contrôleur à partir des seuls timings. Il complète les compteurs d'underrun, de présentation et d'invalidation déjà présents afin de relier une reproduction à sa configuration.

## Tests exécutés

Le test `tools/display-driver-tests-0845/run.py` compile les sources Present/Cache réelles avec les corps exacts de `dpi_panel_draw_bitmap`, `mipi_dsi_dma_trans_done_cb`, `Panel_FrameBufferBase::setRotation` et `_rotate_pixelcopy`.

- 500 trames partielles successives, conservation des pixels hors zone modifiée et image externe exacte, y compris sous la première ligne synchronisée par le pilote.
- Deux intercalages d'interruption dans l'appel réel : avant l'affectation de l'index et après cette affectation mais avant la capture du compteur par Tab5OS.
- 1 000 événements DMA au repos sans nouvelle publication ni synchronisation ; pas d'écriture synchronisée dans le tampon balayé dans les parcours contrôlés.
- 64 combinaisons de rotation/offset, 12 800 rectangles comparés aux méthodes M5GFX exactes.
- Diagnostic disponible après initialisation, indisponible avant initialisation, depuis une autre tâche, pour un autre matériel ou bus ; pas de publication/synchronisation provoquée par le diagnostic.

ASan et UBSan actifs, LSan non exploitable dans cet environnement. Les registres, descripteurs, événements DMA, caches, mémoire physique et géométrie de l'objet M5 restent des adaptateurs hôte explicites. Ce test ne lance pas le périphérique DSI, le pilote complet ni l'OS ; il ne mesure pas la bande passante ou les flashs. Les empreintes des sources et fonctions sont dans `tools/verification/display-driver-0845.json`. Compilation objet des cinq modules d'affichage : `tools/verification/display-target-0845.json`.

## Prochaine mesure nécessaire sur appareil

Sur une page stable puis après quelques clics produisant le flash, relever `d` sans ouvrir Informations système. Comparer les différences de soumissions et d'underruns, vérifier `observed=1` et `active=1`, et conserver la nouvelle ligne de configuration. Les scans augmentent normalement au repos ; les underruns ne devraient pas augmenter dans un fonctionnement sain. Une augmentation d'underrun établira le problème d'alimentation du pont DSI en données, sans encore prouver quel autre accès mémoire l'a provoqué. Une absence de compteur actif ne doit pas être traitée comme un résultat nul.
