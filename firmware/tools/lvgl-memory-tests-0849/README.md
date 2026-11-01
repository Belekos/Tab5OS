# Allocateur LVGL : vérification du contrat

`Tab5LvglMemory.cpp` réutilise l'algorithme TLSF de **LVGL 9.6.0**, sans copie ni modification de cet algorithme. Le tas ESP-IDF fournit uniquement le contrôle TLSF et des segments PSRAM. Les milliers d'objets LVGL et leurs réallocations sont traités à l'intérieur de ces segments, sous mutex de tâche normal. Les copies de `lv_tlsf_realloc` n'ont donc pas lieu sous le verrou global du tas ESP-IDF qui masque les interruptions.

Les segments ordinaires font 64 Kio. Une allocation plus grande obtient un segment adapté ; un échec de réservation de 64 Kio autorise une tentative de taille adaptée à la demande. La marge de taille tient compte de la granularité native des classes TLSF. `LV_MEM_SIZE = 32 Mio` configure cette plage de classes et **ne réserve pas 32 Mio** : le backend reste `LV_STDLIB_CUSTOM`, avec `LV_USE_TLSF=1`.

Un descripteur par segment suit le nombre de blocs vivants. Les pointeurs LVGL ne sont pas déplacés pour compacter : le segment n'est rendu à ESP-IDF que lorsque son dernier bloc est libéré. La recherche du segment d'un pointeur est linéaire dans le nombre de segments, hors section critique masquant les interruptions ; aucun en-tête propriétaire n'est ajouté à chaque allocation. Les pools ajoutés par l'API publique sont empruntés : ils ne sont jamais libérés à ESP-IDF et leur retrait est refusé tant qu'ils contiennent un bloc vivant.

Les appels de mémoire sont sérialisés même entre tâches. Cela ne rend pas les objets LVGL accessibles depuis plusieurs tâches : leur propriétaire reste UI. `lv_mem_init/deinit` suivent le cycle de vie global LVGL, sans utilisateur concurrent pendant initialisation/destruction. Comme l'API native malloc/free, un pointeur intérieur, déjà libéré ou appartenant à un autre allocateur viole le contrat ; ce code ne prétend pas fournir une protection mémoire des applications.

## Exécution

```sh
python3 tools/lvgl-memory-tests-0849/run.py \
  --lvgl /chemin/vers/lvgl-9.6.0-original \
  --build /chemin/temporaire/lvgl-memory-tests --jobs 6
```

Le banc compile LVGL réel avec le `lv_conf.h` de production, son correctif arabe existant, le `Tab5LvglMemory.cpp` de production et AddressSanitizer/UndefinedBehaviorSanitizer. Seules les frontières d'allocation ESP et de mutex FreeRTOS sont simulées. Il ne modifie aucune dépendance installée.

Couverture : échec du contrôle initial ; 10 000 blocs aux tailles différentes et ordre de libération mélangé ; intégrité TLSF ; réduction et agrandissement en place ; déplacement de réallocation avec conservation des octets ; échec de réallocation avec ancien bloc intact ; tailles excessives ; fragmentation du tas global simulée par bloc maximal de 4 Kio ; croissance à 8 Mio ; 105 tailles aux frontières de classes/puis­sances de deux de 15 octets à un peu plus de 16 Mio ; pools externes mal alignés/trop petits/encore utilisés ; quatre threads réels de 600 cycles ; 100 créations, rendus et destructions de panneaux LVGL (6 000 labels), avance du temps LVGL et stabilité mémoire après échauffement ; retour de toutes les réservations au démontage LVGL.

Résultats exécutés sur hôte 64 bits :

- 10 000 allocations : **19 demandes** au tas global, contre une demande par objet dans l'allocateur précédent ; les 19 segments sont ensuite rendus. Charge utile vivante 1 119 832 octets, réservation contrôle compris 1 249 904 octets.
- 100 cycles UI : 6 120 octets avant premier rendu, 6 192 après ; niveau stable après échauffement. Une réserve de 64 Kio reste pour les objets vivants, puis tout est rendu par `lv_deinit()`.
- Les segments de 256 Kio essayés au prototype retenaient 192 Kio de plus pour cette petite UI et 65 536 octets de plus sur les 10 000 allocations ; les segments de 64 Kio ont été retenus.
- Compilation GCC RISC-V ESP32-P4 : contrôle TLSF **2 528 octets PSRAM**, données statiques de l'adaptateur **132 octets**. La taille hôte de contrôle (4 720 octets) ne doit pas être utilisée pour le bilan cible. Preuve : `tools/audit-0849/lvgl-tlsf-size-target.txt`.

## Limites à mesurer sur Tab5

La mémoire libre **à l'intérieur** d'un segment contenant encore un objet ne redevient pas immédiatement disponible pour les autres applications. `ownedBytes - usedBytes` inclut cet espace ainsi que les métadonnées ; une fragmentation durable peut donc réserver davantage de PSRAM que la charge utile. Les segments de 64 Kio réduisent cette granularité, sans promettre de l'éliminer. Les métriques `tab5LvglMemoryStats()` sont des compteurs constants ; le moniteur LVGL décrit désormais son propre tas et non la PSRAM partagée entière.

Les tests hôte valident la propriété, les échecs, le rendu natif et les cycles de ressources. Ils ne mesurent ni les interruptions ESP32-P4, ni le débit PSRAM, ni les flashs physiques ou les claquements audio. Il faut comparer les mêmes parcours avec les compteurs DSI, temps UI/flush et mémoire avant/après sur la tablette.
