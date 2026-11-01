# Affichage 0.8.47 : transfert RGB565 explicite et vérification de la chaîne commune

Le retour matériel sur 0.8.46 établit que les flashs restent présents. Les résultats hôte de cette version ne démontraient pas leur disparition et ne constituent pas une validation physique de la chaîne DSI.

## Axes retenus

1. Un seul propriétaire du rendu et de LVGL : la tâche UI. Déplacer les écritures graphiques sur le cœur audio sans protocole de propriété ajouterait un risque de concurrence et ne supprimerait pas la contention PSRAM commune.
2. Séparer le transfert des pixels de l'interface générique de dessin. Le port LVGL connaît déjà son format RGB565, sa géométrie, son pas de ligne et sa rotation ; la conversion affine générique de M5GFX est inutile dans ce cas précis.
3. Garder une image visible immuable jusqu'à la fin du balayage et publier uniquement une image complètement synchronisée. La barrière IDF existante et ses chemins d'erreur restent conservés.
4. Mesurer séparément coût de transfert, intervalle des callbacks de balayage et underruns DSI. Aucune de ces valeurs n'est assimilée à une preuve que tout flash a disparu.

## Modification réalisée

`Tab5DisplayBlit.h` transfère les pixels RGB565 big-endian produits par LVGL vers l'image arrière RGB565 native. La rotation est effectuée par blocs de 16 × 16 pixels. Un bloc de 512 octets sur la pile sert à lire la source par lignes, puis à écrire la destination par lignes, y compris pour une rotation de 90° : la conversion affine pixel par pixel et ses lectures de colonnes dans l'image source ne sont plus nécessaires.

Les dimensions, la rotation, les tailles disponibles et le pas de ligne sont vérifiés avant toute écriture. Un pas source avec padding et une adresse source non alignée sont acceptés. Le calcul de l'étendue source évite les débordements arithmétiques. Le port utilise le pas réel du buffer LVGL ; le chemin M5GFX de secours conserve aussi ce pas en transférant ligne par ligne lorsque nécessaire.

Ce chemin n'est activé que pendant une transaction du présentateur, sur son image arrière, avec la géométrie RGB565 validée et le suivi de cache disponible. Sans second buffer, sans métadonnées de cache, ou si le mode M5GFX change, le chemin synchrone antérieur reste disponible. Aucun framebuffer supplémentaire, worker graphique, allocation par rectangle ni timer n'est créé.

Le miroir entre images est volontairement conservé. Le supprimer correctement exige de connaître exactement quelles régions seront réécrites avant de recopier les autres. Une approximation par rectangles englobants pourrait perdre des pixels entre deux zones disjointes. Cette livraison ne remplace pas une propriété correcte par une réduction de copies non démontrée.

La taille de frame GCC P4 mesurée est de 576 octets pour `Tab5DisplayBlit::copy` et 64 octets pour son appelant. Cela inclut le bloc de 512 octets et n'est pas une mesure du minimum de pile réel de la tâche UI. Les deux framebuffers DSI, le buffer LVGL partiel et les tables de régions ne changent pas de taille.

## Points fondamentaux réexaminés

Les versions réellement utilisées sont M5GFX 0.2.30, M5Unified 0.2.23 et ESP-IDF 5.5.4. M5GFX configure deux images DSI et leur format RGB565. La configuration P4 conserve PSRAM 200 MHz, cache L2 256 Kio et lignes de 128 octets ; le chemin testé n'active pas XIP PSRAM. Les timings des variantes ILI9881C/ST7123/ST7121 restent ceux de M5GFX, sans changement empirique de fréquence ni réinitialisation périodique.

`Panel_FrameBufferBase::display` synchronise le cache mais ne publie pas de nouvelle image. `Tab5DisplayPresentEndFrame` reste l'unique publication après le rendu. Le callback DMA exact d'IDF 5.5.4 sélectionne le descripteur avant `on_refresh_done`. Deux événements après soumission protègent toujours l'ancienne image visible. Les écritures M5.Display trouvées en dehors de ce port correspondent au démarrage, à l'erreur d'initialisation, à la luminosité et à l'orientation ; aucune écriture directe périodique de pixels après initialisation n'a été identifiée.

Le traitement LCD_RST adapté aux ST7123/ST7121 existe déjà dans les bibliothèques verrouillées. Les fonctions de redémarrage du descripteur DMA sont placées en IRAM par le script de liaison SDK, même lorsque leur déclaration C n'a pas d'attribut IRAM explicite. L'absence de cet attribut n'a donc pas été traitée comme une anomalie. Le nouveau callback de mesure compile en `.iram1` et ses opérations atomiques 32 bits sont des instructions RISC-V, sans appel à une bibliothèque atomique.

La politique AXI existante est inchangée. Son champ DW-GDMA maître mémoire correspond au champ du HAL exact. Aucune modification de priorité n'est introduite sous prétexte que le flash serait nécessairement lié à la bande passante.

IDF signale explicitement qu'un underrun peut rendre l'image bleue. Cela reste un mécanisme possible, pas une preuve de causalité sur cette Tab5. Les écritures NVS et les reconstructions de Média sont traitées dans les autres volets de cette version ; leurs effets sur l'affichage doivent être observés séparément.

## Vérifications exécutées

- Le transfert de production est comparé aux méthodes exactes `setRotation`, `_rotate_pixelcopy`, `writeImage` et à la conversion RGB565 de M5GFX 0.2.30 : 12 288 rectangles, 238 258 340 pixels, les 64 combinaisons rotation/offset sur deux géométries, sources impaires et pas avec padding. Les buffers complets, y compris les pixels inchangés et le padding destination, sont comparés après chaque opération.
- Les refus d'arguments invalides laissent l'image intacte : limites, tailles insuffisantes, débordement de pas, destination non alignée et dimensions extrêmes.
- Le présentateur/cache de production est exécuté avec les corps exacts d'IDF `draw_bitmap` et du callback DMA : 500 trames partielles, 545 publications, intercalages d'interruptions, panne de cache, panne de publication et reprise. Les vues cache CPU, mémoire externe et image balayée sont distinctes dans le modèle.
- Le callback `displayFlush` extrait du fichier principal actuel est exécuté avec LVGL 9.6 réel : 201 450 ms simulées, dont 180 secondes au repos, 97 trames et 96 publications. Pas de nouvelle publication ni synchronisation cache au repos. Les erreurs cache et l'arrêt temporaire des événements DMA conservent l'image visible.
- Les échecs d'allocation de métadonnées et l'indisponibilité du présentateur passent encore par le transfert synchrone antérieur.
- Les cinq unités de production `Present`, `Cache`, `Refresh`, `Signals`, `Bandwidth` compilent avec le compilateur P4 et les dépendances verrouillées. La compilation complète appartient au rapport général.

Preuves : `tools/verification/display-0847.json`, `display-target-0847.json`. Reproduction : `python3 tools/display-tests-0847/run.py --gfx <M5GFX-0.2.30> --baseline <ZIP-0.8.46> --build <repertoire-temporaire>` ; le test d'intégration utilise l'archive LVGL hôte préparée par les outils de compilation. ASan/UBSan sont actifs, LeakSanitizer est indisponible sur l'hôte.

Les octets synchronisés par le cache sont identiques à la référence dans la série comparative. Le changement concerne le calcul et la localité des accès de conversion. Aucun gain de FPS, débit PSRAM, consommation ou disparition des flashs n'est revendiqué sans appareil.

## Lecture des diagnostics sur appareil

La commande série `d` conserve les compteurs existants et ajoute `nativeRects`, `nativePixels`, `maxBlitUs`, `maxRefreshGapUs`. `nativeRects` doit augmenter lorsque le port accéléré est utilisé. Un maximum de transfert élevé avec un intervalle de balayage normal distingue un rendu lent d'une interruption du service de balayage. Un compteur d'underruns qui augmente, avec observation active, établit une sous-alimentation DSI ; l'absence d'observation ne vaut pas absence de panne. Les intervalles d'écriture NVS du volet Média permettent une corrélation supplémentaire, sans en présumer la cause.

À valider sur Tab5 : changements d'onglets Média, interaction Dessin, lecture audio et vidéo, changements d'orientation et période au repos. Tant que les flashs ne sont pas absents dans ces scénarios et que les journaux ne sont pas cohérents, cette partie ne constitue pas une certification de mise en production.
