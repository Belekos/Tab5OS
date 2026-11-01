# Présentation écran : copie différée des différences entre images

## Cause établie dans les sources .47

Le présentateur possède deux images RGB565 gérées par le pilote. Après une permutation, le prochain tampon de dessin doit recevoir les zones modifiées dans l'image précédente. En .47, ces zones étaient copiées dès `BeginFrame`, avant de connaître les zones que LVGL allait remplacer. Une nouvelle page plein écran recopiait donc 1 843 200 octets anciens, puis les écrasait entièrement avec la nouvelle page. Le DMA continuant de lire la PSRAM, ce trafic CPU évitable augmente la concurrence mémoire. Cela établit un coût inutile, pas à lui seul la cause physique de chaque flash.

## Correction .48

- Les anciennes différences sont enregistrées dans deux intervalles physiques par ligne, soit 10 240 octets optionnels en PSRAM. Aucun troisième framebuffer ni nouvelle tâche.
- `DirtyRect`, appelé avant chaque écriture, retire la zone à remplacer de ces différences. Si une ligne demanderait plus de deux intervalles, les différences restantes sont copiées immédiatement avant cette écriture, sans toucher aux pixels nouveaux déjà dessinés.
- À la fin de la transaction, les intervalles encore nécessaires sont copiés, intégrés aux zones de synchronisation du cache, puis le tampon est publié selon la clôture DMA existante.
- Un échec de l'allocation des métadonnées conserve la copie immédiate de .47. Les échecs de cache/publication conservent l'image de devant et demandent la reconstruction de l'image suivante.
- Quand le suivi des écritures est explicitement invalidé, la transaction différée est rejetée. Le hook faible M5GFX ne peut cependant pas détecter toute écriture non annoncée après un rectangle valide : le contrat interne reste « annoncer chaque zone avant de l'écrire ». Les applications ELF natives ne bénéficient pas d'une isolation mémoire.

## Tests réellement exécutés le 28 octobre 2026

`python3 tools/display-tests-0848/run.py --baseline <sketch .47>` exécute les sources de production Present/Cache, les corps exacts de `dpi_panel_draw_bitmap` et du callback DMA IDF 5.5.4, et les transformations M5GFX 0.2.30. Les frontières matériel/cache/IRQ sont contrôlées par le test hôte, avec vues séparées mémoire CPU, RAM visible et tampon scanné. ASan et UBSan sont activés.

- 500 images partielles, puis 20 images à bandes multiples vérifiant la fragmentation dans le présentateur complet ; 64 combinaisons orientation/offset et 12 800 rectangles ; comparaison pixel par pixel ; entrelacements callback/publication ; échecs cache/publication ; contrôle du maintien du tampon scanné.
- 20 000 transactions de ligne aléatoires contre un oracle de pixels, dont 856 évacuations pour fragmentation : aucun pixel nouvellement écrit n'est remplacé par l'ancien.
- Métadonnées facultatives absentes, cache absent, second framebuffer absent : replis existants opérationnels dans le modèle.
- 1 000 callbacks DMA au repos n'ajoutent ni publication ni synchronisation de cache.

Après le même amorçage plein écran, 40 changements plein écran donnent :

| Compteur logiciel | .47 | .48 |
|---|---:|---:|
| Octets recopiés depuis l'image précédente | 73 728 000 | 0 |
| Octets soumis au writeback du cache | 73 785 600 | 73 785 600 |

La quantité synchronisée n'est donc pas présentée comme un gain : seule la copie CPU redondante est supprimée. Les totaux globaux du test diffèrent parce que .48 exécute aussi un scénario supplémentaire de rejet d'écriture non suivie ; ils ne constituent pas une comparaison de performance.

Les journaux et empreintes des sources sont dans `tools/verification/display-0848.json` et `tools/verification/display-0848/`. Aucun FPS, débit PSRAM ou flash visuel n'a été mesuré sur une Tab5 physique. La compilation du firmware et les essais physiques sont suivis séparément dans le compte rendu global.
