# Revue indépendante du miroir différé — 0.8.48

Relecture de `Tab5DisplayPresent.cpp`, `Tab5DisplayCache.cpp`, `Tab5DisplayCacheRanges.h`, `Tab5DisplayPendingMirror.h` et du `displayFlush` LVGL du sketch, comparés à l'archive .47 authentifiée. Aucun défaut nouveau identifié dans le périmètre relu.

- Le miroir ne touche que le back buffer, après libération de la barrière de présentation existante. Le front reste en lecture seule.
- Le bitmap logique des zones restantes est borné à deux intervalles par ligne physique. Le dépassement recopie uniquement les restes avant l'écriture courante ; les zones déjà écrites ont été retirées.
- `displayFlush` annonce la zone avant le blit natif ou le repli M5GFX. Les lignes restantes sont copiées avant la synchronisation du cache et avant la publication.
- Un refus de synchronisation ou de publication réclame un miroir et une invalidation complets. Une allocation refusée pour les 10 Kio de métadonnées conserve le miroir immédiat antérieur.
- Le changement de rotation ne réinterprète pas les différences précédentes : elles sont mémorisées en coordonnées physiques.

Test indépendant : `review_pending_mirror.cpp` énumère 474 552 combinaisons d'un intervalle initial de différence et de deux écritures, ajoute une troisième écriture centrale, puis termine le miroir. L'oracle construit le résultat pixel par pixel ; toute recopie qui touche un pixel déjà réécrit est interdite. Résultat exact et absence d'accès invalide sous ASan/UBSan, journal `tools/verification/display-pending-independent-0848.json`.

Limites : test d'arithmétique et revue de l'intégration, pas de mesure de DMA ou de flash visible sur appareil. Le hook générique de cache ne peut pas détecter toutes les écritures étrangères non annoncées lorsqu'une autre zone a déjà été annoncée dans le même buffer. Le contrat annonce-avant-écriture reste nécessaire ; aucun appel livré qui l'enfreint n'a été identifié. Les ELF ne sont pas isolés matériellement de ces buffers.
