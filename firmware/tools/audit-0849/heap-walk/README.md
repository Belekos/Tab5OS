# Coût du parcours de heap dans ESP-IDF 5.5.4

Ce banc exécute `multi_heap_get_info_impl` et `tlsf_walk_pool` des sources
Espressif épinglées, sans remplacer leur algorithme par une valeur de mémoire
libre constante. Le verrou de plateforme est instrumenté à son entrée et sa
sortie. Le linker observe chaque visite du walker et vérifie que toutes sont
faites sous verrou. Le chemin `multi_heap_free_size` est exécuté séparément.

```sh
python tools/audit-0849/heap-walk/run.py
python tools/audit-0849/heap-walk/run.py --sanitize
```

Les résultats sont enregistrés dans `results.json` et
`results-sanitized.json`. Seuls le nombre de blocs visités et les invariants
du verrou sont des critères de réussite. Les durées sont informatives et
proviennent de l'hôte, **pas de la Tab5**.

L'arène hôte fait 64 Mio : cela maintient l'alignement des métadonnées du
TLSF Espressif en ABI LP64. L'essai initial avec une arène 32 Mio échouait
dans `tlsf_add_pool` avant toute mesure, à cause du placement de ces
métadonnées en ABI 64 bits ; aucune conclusion ne doit être tirée pour
l'ESP32-P4 32 bits. Aucun fichier upstream n'a été modifié. La compilation
hôte n'utilise pas LIGHT poisoning, contrairement au firmware ; le wrapper
de `get_info` du firmware appelle néanmoins cette même implémentation.

Les fichiers `upstream/` proviennent de :

- [ESP-IDF v5.5.4, components/heap](https://github.com/espressif/esp-idf/tree/v5.5.4/components/heap)
- [TLSF, sous-module épinglé par ce tag](https://github.com/espressif/tlsf/tree/2867f6883a12920b1969ff9624c0ab0e4185c2ce)

Leurs licences et leurs notices ont été conservées. Les SHA-256 sont inclus
dans les résultats. Le chemin de verrou a aussi été vérifié dans le
`libheap.a` réellement livré avec `esp32p4_es-libs/3.3.9`, par désassemblage :

`heap_caps_get_largest_free_block → heap_caps_get_info → multi_heap_get_info_impl
→ multi_heap_internal_lock → xPortEnterCriticalTimeout`, puis
`tlsf_walk_pool`, puis `multi_heap_internal_unlock`.

Ce banc ne simule ni le contrôleur d'interruptions, ni le DMA, ni le cache
du P4. Il démontre le travail effectué dans la section critique et son
augmentation avec le nombre de blocs. Il ne démontre pas à lui seul que ce
travail est la cause des flashs physiques.
