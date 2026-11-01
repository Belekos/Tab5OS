# Revue indépendante des corrections mémoire

## Périmètre

Lecture indépendante de `Tab5HeapProbe.c`, `Tab5HeapFit.h`, des admissions dans `Tab5ResourceManager.cpp` / `Tab5Memory.cpp`, puis de `Tab5LvglMemory.cpp`. Aucun changement de ces sources par le relecteur. Cette revue porte sur les contrats logiciels; elle n'établit pas les performances ou la stabilité physique de la tablette.

## Sonde d'admission

La sonde reprend la recherche de classe TLSF et la lecture de bitmaps du SDK, sans retirer de bloc de liste libre. La comparaison avec `heap_caps_aligned_alloc_base`, `multi_heap_poisoning.c` et `tlsf_memalign_offs` IDF 5.5.4 confirme :

- alignement initial de quatre octets puis adaptation native `esp_heap_adjust_alignment_to_hw`;
- restriction aux capacités réellement utilisées par les admissions; les autres retournent UNKNOWN;
- correspondance de capacités à travers les trois priorités de chaque région;
- surcoût LIGHT de douze octets sur P4 : en-tête huit et fin quatre;
- pour une allocation alignée, l'offset d'en-tête s'annule dans la taille de recherche de `tlsf_memalign_offs`; le supplément utilisé par la sonde est cohérent;
- vérification des dépassements et de l'indice de classe avant accès aux tables;
- verrou natif maintenu pendant la lecture des métadonnées, puis relâché avant retour.

La sonde reste un instantané. Une autre tâche peut consommer le bloc après le retour; l'allocation effective reste donc l'autorité et son échec doit être traité. Le gestionnaire n'affiche pas un faux « plus grand bloc » : les états disponible, indisponible et non mesuré sont distincts. Une configuration non couverte utilise l'interrogation publique du SDK et conserve le fonctionnement antérieur.

Le recours à une ABI privée constitue une dépendance de maintenance réelle. Une vérification du seul numéro de version ne suffit pas pour une archive SDK arbitrairement recompilée. La revue a donc extrait le DWARF de **l'archive réellement utilisée** :

| Structure native | Taille | Offsets concernés |
|---|---:|---|
| `heap_t_` | 36 | caps 0; start 12; end 16; verrou 20; heap 28; next 32 |
| `multi_heap_info` | 20 | verrou 0; free 4; minimum 8; taille 12; heap_data 16 |
| `block_header_t` | 16 | précédent 0; taille 4; libre suivant 8; libre précédent 12 |
| `control_t` | 36 | métadonnées décrites par les headers TLSF natifs |

Les deux headers TLSF utilisés par les tests sont identiques octet par octet à ceux du SDK de compilation. Le script indépendant `review_heap_abi.py` vérifie ces dispositions et le hash de `libheap.a`; preuve dans `tools/verification/heap-abi-independent-0849.json` et son extrait DWARF `.txt`. Des assertions de compilation complémentaires sur les offsets effectivement lus ont été recommandées à l'auteur du correctif.

## Essais effectivement relancés

Le relecteur a exécuté `tools/memory-admission-tests-0849/run.py --db build_0848/build/compile_commands.json` : **5 000 comparaisons contre allocations TLSF réelles, 10 022 vérifications, aucun échantillonnage exhaustif du heap dans les admissions testées**. Le banc vérifie aussi la non-modification octet par octet du heap lors de la sonde et le refus puis la récupération d'un heap fragmenté. Les trois unités cible `Tab5HeapProbe.c`, `Tab5ResourceManager.cpp`, `Tab5Memory.cpp` compilent.

Le TLSF hôte fonctionne en ABI LP64, tandis que le P4 utilise des pointeurs 32 bits. L'exclusion du contrôle UBSan d'alignement dans ce banc est explicitement documentée par son auteur; elle ne constitue pas un essai du P4. L'adaptateur privé P4 est compilé pour la cible et ses dispositions binaires sont vérifiées séparément. La compilation hôte a émis un avertissement de format dans `multi_heap_dump_tlsf` amont (`size_t` LP64 avec `%d`); cette fonction n'est pas exécutée par le banc.

## Allocateur LVGL par pools libérables

La lecture de `Tab5LvglMemory.cpp` n'a pas révélé de défaut dans les points suivants : compteurs d'allocations vivantes par pool, changement de pool lors d'une réallocation, retrait uniquement d'un pool vide, conservation de l'ancien bloc sur échec, repli vers un segment plus petit si 64 Kio ne sont pas disponibles, et conservation de la propriété des pools externes.

La copie d'une réallocation TLSF se déroule sous un mutex de tâche, qui ne masque pas les interruptions, et non sous le verrou du heap ESP. Les segments vides appartenant à l'adaptateur sont rendus au heap partagé; il n'est pas nécessaire de réserver un pool fixe de deux Mio. Le contrôle TLSF reste alloué tant que LVGL est initialisé, ce qui est cohérent avec son cycle de vie.

Cette seconde partie est une revue de code, pas une nouvelle campagne de tests indépendante. Les tests fonctionnels et de propriété de l'auteur de l'allocateur sont consignés séparément. La fragmentation, les pics effectifs, l'impact de la recherche du pool contenant un pointeur et les délais d'affichage restent à vérifier sur cible.

## Conclusion de revue

Aucun défaut bloquant confirmé dans les sources examinées. Les limites matérielles et la dépendance à l'ABI privée restent explicites; la validation finale doit inclure la vérification de l'archive SDK, le lien du firmware et les essais sur Tab5. Ce résultat ne permet pas d'annoncer la disparition des flashs.
