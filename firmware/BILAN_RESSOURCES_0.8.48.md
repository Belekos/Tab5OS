# Ressources et validation — Tab5OS 0.8.48

## Compilation réellement exécutée

Compilation complète Linux : Arduino CLI 1.5.1, paquet M5Stack 3.3.9 / ESP-IDF 5.5.4, GCC RISC-V 14.2, M5Unified 0.2.23, M5GFX 0.2.30, LVGL 9.6.0. Les 23 archives de dépendances et l'archive CLI ont été vérifiées par empreinte. Les corrections M5Unified et DSI/DW-GDMA sont appliquées dans des copies isolées ; les bibliothèques installées restent intactes.

Le dernier build a terminé sans erreur. Une comparaison indépendante des avertissements de sept unités concernées retrouve les mêmes diagnostics dans les sources avant/après ; les avertissements existants ne sont pas assimilés à des corrections. Le main brut et le main généré Arduino sont distingués dans cette comparaison. Les manifestes avant/après et les copies Arduino correspondent aux sources livrées : 2 030 fichiers contrôlés, 1 918 copies contrôlées et 1 423 objets. Les scripts Windows sont intégrés et contrôlés statiquement ; aucune exécution Windows n'est revendiquée.

Le BIN applicatif vérifié porte le SHA-256 `bfb09d79857a6c6bab18ced7726f3bc1c947a1a2fd2633d341731f1c8ce20fc1`. Il est destiné à l'OTA, pas à un flash à l'adresse zéro. Le partitionnement est inchangé. Les ELF et MAP de diagnostic exacts sont joints au projet.

## Mesures dans les binaires

La référence 0.8.47 est le binaire livré, dont les empreintes ont été revérifiées ; elle n'a pas été recompilée intégralement dans cette reprise.

| Mesure, en octets | 0.8.47 | 0.8.48 | Écart |
|---|---:|---:|---:|
| Image applicative BIN | 6 080 080 | 6 084 512 | +4 432 |
| Capacité d'une partition OTA | 6 553 600 | 6 553 600 | 0 |
| Marge dans la partition OTA | 473 520 | 469 088 | −4 432 |
| Données internes statiques et BSS | 93 644 | 93 848 | +204 |
| Section `.iram0.text` | 98 660 | 98 538 | −122 |
| `.tcm.text` / `.tcm.data` | 794 / 64 | 794 / 64 | 0 |
| Journal `.rtc_noinit` | 1 200 | 1 200 | 0 |

« Application / partition OTA » décrit l'occupation de **Flash** : ici environ 5,80 Mio sur 6,25 Mio. Ce n'est pas la RAM libre. Les sections statiques ne permettent pas de déduire seules le heap disponible après démarrage : tâches du SDK, pilotes, caches et allocations dynamiques interviennent ensuite.

La mémoire interne nécessaire aux métadonnées DW-GDMA augmente légèrement pour rester accessible pendant les opérations Flash. Les 32 Mio de PSRAM ne peuvent pas satisfaire cette contrainte à sa place. Les catégories INTERNAL, 8BIT et DMA se recouvrent ; elles ne doivent pas être additionnées comme trois réserves. Les chiffres exacts du heap libre, minimum et plus grand bloc restent à relever sur l'appareil.

## Mesures instrumentées dans les tests

| Scénario | Avant | Après | Portée |
|---|---:|---:|---|
| Miroir de 40 images complètes après le même amorçage | 73 728 000 octets copiés | 0 | Présentateur/cache réels sur hôte |
| Writeback de ces mêmes images | 73 785 600 octets | 73 785 600 | Aucun gain de writeback revendiqué |
| Début pinceau sur 1280 × 720 | 3 686 768 octets copiés | 384 | Instrumentation hôte, métadonnées incluses |
| Capture du premier point de ce geste | Calque déjà copié | 2 048 octets | Capture des blocs touchés |
| Mise à zéro CPU de trois images vidéo 720p | 5 529 600 octets | 0 | Même alignement/capacités, publication après décodage réussi |
| Probe de 100 JPEG à sampling invalide | 100 allocations retenues | 0 allocation | Reproduction du défaut SDK, pas une mesure de SRAM physique |
| Reprises musique → arrêt → silence contaminées | 192 / 192 | 0 / 192 | Mélangeur M5Unified réel ; 16 384 échantillons continus inchangés |
| Créations de worker audio sur 600 s de Wi-Fi stable | 120 | 1 | Temps simulé ; aucune mesure de CPU réel |
| Callbacks DW-GDMA sur 12 000 événements cache-off | 0 | 12 000 | Driver natif entier, modèle IRQ/HAL contrôlé |

Coûts assumés : 10 Kio optionnels de PSRAM pour le miroir différé ; bitmap d'historique Dessin de 450 octets par geste 1280 × 720 ; `IEState` et `IEDoc` grandissent chacun de 8 octets sur P4. La pile locale de `state_swap` passe de 320 à 656 octets. La réservation maximale des pixels d'historique reste conservée : la réduction des copies ne doit pas être assimilée à une réduction identique de mémoire réservée.

## Tests et revues

- Affichage : images complètes et partielles, rotations, fragmentation des zones, erreurs cache/publication, métadonnées indisponibles et revue indépendante des pixels. Le vérificateur du binaire final réussit 48 contrôles IRAM/DRAM/IRQ et détecte les défauts attendus sur le binaire précédent.
- Audio : 192 reprises, continuité PCM, scénarios d'échec de démarrage et 1 000 reprises après repos. Politique Wi-Fi testée avec commandes prioritaires, panne RPC, radio en pause, reconnexion et demande pendant RPC.
- Vidéo : 16 JPEG valides, 100 000 entrées déterministes, 9 651 parcours des parseurs CPU natifs ; OOM et sorties partielles sur 200 cycles de buffers ; 208 passages du cycle worker, dont 200 remplacements réutilisant une tâche. Les I/O et le scheduler sont contrôlés dans le banc de cycle ; aucun stream réseau physique n'y est lu.
- Dessin : 4 000 opérations comparées à .47, erreurs mémoire, pixels non capturés empoisonnés sous ASan, undo/redo/annulation ; UI LVGL réelle avec 38 rendus et cinq fermetures sans allocation PSRAM suivie restante.
- Persistance Média : 400 cycles, un seul job simultané, zéro écriture NVS depuis l'UI du modèle, format enregistré identique à .47. Coexistence radio/Cartes : tests du verrou réseau réexécutés, 1 835 contrôles par version.
- Langues : 14 packs conservés ; aucune nouvelle chaîne utilisateur à traduire. Ressources embarquées et Factory vérifiées. Les copies temporaires reconnues sont exclues de l'archive sans retirer les données applicatives ni les preuves historiques authentiques.

Les rapports précisent leurs empreintes, commandes, scénarios et limites. ASan/UBSan sont utilisés dans les bancs concernés ; LeakSanitizer n'est pas utilisable dans cet environnement. Les allocations concernées sont suivies explicitement. Une absence de fuite dans ces cycles n'est pas une preuve d'absence de toute fuite dans l'OS.

## Mesures encore indisponibles

Pas de mesure physique avant/après du heap interne ou PSRAM au repos, de la fragmentation après Cartes, du minimum de pile en usage réel, du temps de démarrage, du CPU, de l'énergie, du débit SD/RF, du son ou du scintillement. La procédure `TESTS_TAB5_0.8.48.md` couvre ces points. Aucun résultat de production n'est revendiqué avant cette qualification.
