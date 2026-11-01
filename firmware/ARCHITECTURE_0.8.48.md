# Organisation des corrections 0.8.48

La structure existante est conservée : UI propriétaire de LVGL, services FreeRTOS, pilotes ESP-IDF, M5Unified/M5GFX, interpréteur MicroPython et chargeur ELF. La priorité de cette étape est de réparer les contrats entre ces couches.

| Couche | Propriété et mémoire | Correction |
|---|---|---|
| DSI / DW-GDMA | Code IRQ en IRAM, contexte et compteurs internes, pixels en PSRAM | Reconstruire les unités natives avec les vrais attributs d'interruption/cache ; aucune IO dans le hook d'underrun |
| Présentation | UI et fences du pilote ; deux framebuffers existants | Ne recopier l'ancien contenu que là où il reste utile |
| Audio | Worker et mélangeur ; état par canal | Réinitialiser l'interpolation après arrêt effectif, conserver volume/pause |
| Vidéo | Un worker propriétaire du décodeur et de ses buffers | Réutiliser le worker pour un remplacement déjà demandé après nettoyage ; probe borné ; éviter les écritures inutiles |
| Dessin | Document et historique propriétaires des pixels | Réserver avant mutation, capturer seulement les blocs modifiés |
| Wi-Fi au repos | État publié acquire/release ; demande RSSI atomique | Supprimer la recréation périodique du worker sans travail utile |

La PSRAM sert aux documents, images, caches et grandes structures compatibles. La mémoire interne sert notamment aux données accessibles pendant les interruptions/cache-off et aux piles qui ne peuvent pas être externes. Une petite allocation de contrôle interne peut être nécessaire même lorsqu'il reste des dizaines de Mio de PSRAM.

Les capacités exposées par `heap_caps_*` se recouvrent : INTERNAL, 8BIT et DMA ne sont pas des réserves indépendantes à additionner. DRAM et IRAM décrivent également des usages ou des vues de mémoire interne ; le heap utilisable ne correspond pas à la seule capacité annoncée de la puce. TCM, cache L2 et LP RAM ne sont pas des extensions interchangeables du heap général. Le diagnostic doit conserver les catégories et montrer le plus grand bloc disponible en plus du total libre.

Ajouter une tâche, utiliser l'autre cœur ou arrêter les invalidations LVGL ne suspend pas les lectures DMA du framebuffer. C'est pourquoi la correction traite à la fois les interruptions, la propriété des buffers et le trafic de copies. Les contraintes du SDK exact sont vérifiées dans le binaire, pas seulement dans les macros du sketch.

Les ELF natifs partagent l'espace mémoire du firmware. Les vérifications de format et d'ABI n'offrent pas une isolation matérielle entre applications. Aucun déplacement de partitions ni changement de chiffrement n'est introduit.

Voir `AUDIT_0.8.48.md` pour les causes établies et `TESTS_TAB5_0.8.48.md` pour la qualification restante.
