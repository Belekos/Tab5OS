# Revue indépendante du transfert graphique 0.8.47

Cette revue concerne le code actuel `Tab5DisplayBlit.h`, `Tab5DisplayPresent.cpp`,
`Tab5DisplayCache.cpp`, `Tab5DisplaySignals.cpp` et le callback `displayFlush` du
sketch. Elle est distincte des tests écrits par l'auteur du transfert. Aucun
résultat présenté ici ne démontre l'absence de flash sur une Tab5 physique.

## Conclusions du code

- L'entrée LVGL est configurée en RGB565_SWAPPED. La reconstruction explicite
  des deux octets vers un mot natif est cohérente avec le framebuffer RGB565
  de M5GFX/DSI. Les huit rotations et symétries ont été vérifiées avec un oracle
  scalaire écrit séparément.
- Le callback utilise le stride du buffer LVGL actif. Dans le LVGL fourni,
  `draw_buf_flush` appelle `flush_cb` avant de changer `disp->buf_act`, et lui
  transmet `layer->draw_buf->data`. Le mode actuel est PARTIAL : le buffer actif
  correspond donc bien à l'entrée du callback. Le nombre d'octets minimal inclut
  le dernier pixel, sans exiger le padding après la dernière ligne. Le fallback
  M5GFX utilise des lignes séparées si le stride contient du padding.
- Le nouveau transfert CPU n'est autorisé que pour le propriétaire UI, pendant
  un frame actif, vers le buffer arrière connu du présentateur et avec le
  collecteur de plages du cache disponible. Le fallback conserve le chemin
  M5GFX si cette préparation manque. Le transfert ne publie pas directement le
  framebuffer.
- Les copies du frame précédent et les nouveaux rectangles sont réunis dans
  les plages à écrire en mémoire. Le cache est synchronisé avant publication.
  Une erreur de cohérence retient le frame et demande un nouveau dessin. Le
  front reste en lecture seule. La barrière existante attend deux événements
  postérieurs à la publication avant réutilisation de l'ancien front.
- Le code actuel du projet ne change pas de viewport/clip M5GFX pendant les
  flush LVGL. Le transfert direct n'ignore donc pas un état de découpage utilisé
  par une application existante. Cette condition reste à revoir si l'API M5GFX
  est exposée à de nouveaux appels concurrents ou à de nouveaux viewports.
- Le compteur d'événements DSI et le seqlock utilisent des atomiques 32 bits
  sans verrou. Les données de l'observateur sont explicitement internes et le
  callback est IRAM. Il observe le statut sans l'acquitter ; le pilote natif
  conserve l'acquittement et le traitement. La présence effective des fonctions
  SDK en IRAM est contrôlée séparément par `verify_flash_context.py --elf`.
  Dans l'archive `libesp_timer.a` du SDK utilisé, `objdump -t` place aussi
  `esp_timer_get_time` et `esp_timer_impl_get_time` dans `.iram1.1` ; le nouvel
  horodatage de refresh n'ajoute donc pas un appel de timer stocké en Flash.

Aucun défaut bloquant n'a été identifié dans ce périmètre. Les lectures de
diagnostic ne doivent pas être ajoutées à une ISR de priorité supérieure au
writer du seqlock : les usages présents sont dans des tâches.

## Vérification exécutée

`review_display_blit.py` compile le vrai header de production et
`review_display_blit.cpp` avec ASan/UBSan. Le fixture n'utilise ni la formule de
transformation du header ni celle des tests de son auteur.

- 8 000 rectangles générés, répartis sur les huit transformations.
- 1 919 663 pixels comparés avec un oracle indépendant.
- Entrées non alignées ; strides avec padding pair ou impair ; dimensions
  variables ; extension source minimale, sans padding de dernière ligne.
- 32 000 refus vérifiés sans mutation de la destination : source tronquée,
  stride insuffisant, destination tronquée et géométrie incohérente.
- AddressSanitizer et UndefinedBehaviorSanitizer : aucun diagnostic.
  LeakSanitizer n'est pas activé ; aucune conclusion matérielle n'est tirée.

Commande depuis le dossier de livraison :

```sh
python3 tools/audit-0847/review_display_blit.py --out tools/verification/display-blit-independent-0847.json
```

Le JSON contient les empreintes des sources testées. Le test ne remplace pas les
tests de présentateur/cache avec injection d'erreurs, la compilation croisée,
ni les observations DSI sur appareil. Le gain de temps sur ESP32-P4 et la
disparition éventuelle des flashs restent à mesurer sur matériel.
