# Ressources — Tab5OS 0.8.44

Comparaison du binaire final avec la livraison vérifiée 0.8.43, même matériel cible et même SDK : M5Stack Arduino 3.3.9, ESP-IDF 5.5.4, GCC RISC-V 14.2, M5Unified 0.2.23, M5GFX 0.2.30 et LVGL 9.6.0. Aucun chiffre de tas libre, CPU, autonomie, FPS ou démarrage sur tablette n'est inventé.

## Mesures du binaire réellement lié

| Mesure | 0.8.43 | 0.8.44 | Évolution |
| --- | ---: | ---: | ---: |
| Image OTA applicative, octets | 6 062 352 | 6 065 728 | +3 376 |
| Partition OTA, octets | 6 553 600 | 6 553 600 | inchangée |
| Marge dans cette partition, octets | 491 248 | 487 872 | −3 376 |
| Occupation de la partition | 92,50 % | 92,56 % | +0,06 point |
| Données internes statiques + BSS, octets | 93 252 | 93 268 | +16 |
| `.dram0.data`, octets | 31 364 | 31 364 | 0 |
| `.dram0.bss`, octets | 45 220 | 45 220 | 0 |
| `.dram1.bss`, octets | 16 668 | 16 684 | +16 |
| `.iram0.text`, octets | 98 494 | 98 494 | 0 |
| `.tcm.text` / `.tcm.data`, octets | 794 / 64 | 794 / 64 | 0 |
| `.rtc_noinit`, octets | 1 200 | 1 200 | 0 |

Le bilan mémoire statique augmente légèrement. La version ne promet pas un gain de SRAM au démarrage. Les changements portent sur la propriété des ressources, les erreurs et le coût transitoire des traitements. La table de partitions NVS/OTA/SPIFFS/coredump est identique à la référence ; aucune migration destructive n'est introduite.

SHA-256 du BIN : `c1d493d821b86925146fa253c936ab3907eddf1c47b27039a722e9fa5afdd92a`. Rapport source/binaire : `tools/verification/firmware-verification-0844.json` et `tools/audit-0844/build-baseline.json`.

## Travail évité et ressources transitoires

| Essai contrôlé sur ordinateur | Référence 0.8.43 | Correction 0.8.44 |
| --- | ---: | ---: |
| Lecteur LVGL au repos 60 s : demandes d'instantané | 235 | 0 |
| Même essai : forçages de l'indicateur du bandeau | 235 | 0 |
| Même essai : invalidations / transferts LVGL | 0 / 0 | 0 / 0 |
| Observateur audio isolé au repos 60 s : copies logiques de String | 1 440 | 0 |
| Observateur isolé : prises du verrou par le lecteur | 240 | 0 |
| Progression moteur à environ 20 Hz : instantanés appliqués au lecteur sur 60 s | 234 | 234 |
| 1 000 décisions réseau de cartes : copies de String | 6 000 | 0 |
| Attente volontaire après requête cartographique annulée/périmée, ms simulées | 60 ou 220 | 0 |
| Demandes injustifiées de récupération SD, dans 48 erreurs injectées | 32 | 0 |

Les mesures du lecteur et de l'observateur isolé sont des fixtures différentes. Les 235 passages viennent d'un pas de 8 ms franchissant l'ancien seuil de 250 ms à 256 ms. Les copies logiques ne sont pas des allocations physiques mesurées. Le plafond d'environ 4 Hz concerne les progressions ordinaires ; les actions forcées et notifications vidéo peuvent être appliquées immédiatement.

La pile du worker cartes reste de **16 Kio internes**. L'annulation peut libérer cette ressource plus tôt ; le modèle ne mesure pas le temps réel de récupération par FreeRTOS. Un appel DNS/TCP/TLS déjà engagé peut encore attendre son délai pilote. L'admission radio reste protégée par ses marges et besoins de blocs contigus.

Le raccourci de bail audio depuis l'état entièrement inactif évite de créer une pile audio de **16 Kio** uniquement pour acquitter une vidéo silencieuse. Le test vérifie ce cas même quand toute admission de tâche est refusée. Il ne réduit ni la pile d'un audio actif, ni la pile vidéo de **24 Kio**, ni les besoins DMA. Les 400 cycles de workers testés terminent avec zéro pile simulée et zéro réservation restantes.

Aucune tâche permanente ni troisième framebuffer n'est ajouté. Le diagnostic des étapes vidéo réutilise le record RTC existant de 8 octets. Le relevé série d'affichage est uniquement à la demande.

## Lire correctement les régions mémoire

Les 768 Kio physiques du P4 ne sont pas un tas interne de 768 Kio libre : le SDK configure notamment 256 Kio de cache L2 ; code, données, réservations, piles et contraintes de capacités occupent d'autres parties. Le total INTERNAL exposé par le SDK peut contenir des sous-régions TCM/SPM ou LP ; ces catégories ne doivent pas être additionnées une deuxième fois.

La PSRAM de 32 Mio est adaptée aux documents, images, tables, caches et allocations compatibles. Elle ne remplace pas tous les blocs internes contigus, piles ou mémoires DMA. Le C6 possède sa propre mémoire et son firmware de communications ; il n'augmente pas le tas du P4. Les politiques existantes de PSRAM prioritaire et d'allocateur TLS sont conservées.

Pour diagnostiquer un refus après cartes, relever `m` avant l'ouverture, après fermeture puis après la tentative radio. Comparer mémoire interne libre/minimale et plus grand bloc allouable, DMA et PSRAM séparément. Relever les lignes d'admission et les marges de pile imprimées par les workers. Une grosse PSRAM libre ne permet pas de conclure que la pile interne ou le DMA étaient disponibles.
