# Affichage au repos — audit 0.8.43

## Référence et résultat

Les sources de présentation/cache/DSI sont identiques à la livraison 0.8.42 vérifiée. Aucun défaut nouveau démontré dans ces modules ne justifie une nouvelle modification de synchronisation, de priorité matérielle ou de récupération. Les corrections de l'accueil et du catalogue sont réalisées dans leurs modules ; elles ne sont pas attribuées au pilote d'écran.

La suite hôte a été rejouée sur les sources 0.8.43, avec LVGL 9.6 réel, ASan/UBSan, doubles explicites de DMA/cache/IRQ et vérification de chaque pixel du framebuffer balayé. Les scripts et empreintes actuels sont dans `tools/display-tests-0843` et `tools/verification/display-0843.json`.

Sur **180 secondes simulées** avec mise à jour chaque seconde du libellé RTC caché des réglages : **0 publication d'image**, **0 écriture cache**, mais **11 250 événements de balayage DMA**. Le balayage DSI continue matériellement pour entretenir l'écran ; ce n'est pas un redessin LVGL ni une boucle de rafraîchissement applicative. Il ne faut donc pas inférer un redessin inutile de la seule présence d'événements DSI.

Les essais couvrent aussi les petites mises à jour visibles, délai de tâche UI, arrêt/reprise DMA, erreur cache/publication, contre-valeurs et pression temporaire de mémoire. Aucun pixel du framebuffer encore balayé ne peut être modifié par le modèle de pipeline. Cela ne mesure pas la bande passante ni les signaux de la Tab5.

## Chemins effectivement examinés

- `Tab5DisplayRefresh` vérifie la barrière avant que LVGL consomme les régions invalidées ; il ne force pas de nouvelle invalidation saine. Un panneau retardé suspend son seul timer de rendu, sans immobiliser les entrées et les autres timers.
- `Tab5DisplayCache` et `Tab5DisplayPresent` ne recopient et ne publient qu'après un rendu demandé. La copie de miroir concerne les pixels de la présentation précédente. Il n'y a pas de copie plein écran périodique au repos.
- Les reprises à une seconde sont conditionnées par une erreur cache/publication précédente. Elles restent nécessaires pour restituer une image qui n'a pas été synchronisée ; les retirer masquerait une panne. Aucune réinitialisation d'écran périodique n'a été trouvée.
- Le BSP M5GFX 0.2.30 initialise deux framebuffers DSI en PSRAM. Son `Panel_FrameBufferBase::display` ne fait rien si sa région modifiée est vide. Les écritures cache restent bornées et leur sémantique C2M n'est pas remplacée par une invalidation dangereuse.
- M5Unified 0.2.23 configure la ligne `LCD_RST` du PI4IO 0x43 en entrée avec pull-up. La table de configuration est appliquée au démarrage ; `M5.update()` entretient notamment les entrées tactiles et ne réapplique pas cette table de reset à chaque tour. Une activité I2C ne suffit pas à prouver un reset LCD.

Les empreintes des fichiers SDK examinés et des modules inchangés sont conservées dans `display-idle-review-0843.json`. Aucun contournement du BSP ni accès supplémentaire aux registres au repos n'est introduit.

## Fluidité et les deux processeurs

La tâche graphique du P4 reste propriétaire des objets LVGL et des deux buffers ; copier ces objets depuis un autre cœur sans protocole d'appartenance créerait une course. Déplacer les lectures de catalogue et la préparation compatible sur l'autre cœur est traité par les lots accueil/mémoire. Les deux cœurs P4 partagent toujours le même bus mémoire : un déplacement de tâche ne supprime pas à lui seul une contention PSRAM/DSI. Le C6 assure déjà les communications ; il n'est pas un cœur SMP du P4 et ne peut pas exécuter directement une tâche LVGL accédant à ses pointeurs PSRAM.

La barrière conservatrice de **deux événements DSI** héritée de 0.8.42 est maintenue. Elle évite qu'un événement associé à une ancienne décision DMA autorise trop tôt l'écriture dans l'ancien buffer. Elle peut limiter la cadence visuelle sous mise à jour continue, même quand les entrées sont traitées rapidement. Une réduction à un événement exigerait de prouver l'ordre SDK et l'affinité des interruptions, puis de valider sur écran réel ; aucune cadence/FPS matérielle n'est annoncée ici.

## Défauts restant à départager sur appareil

1. Activer brièvement le profilage UI, laisser deux minutes l'accueil sans interaction, puis relever les compteurs d'invalidation/publication, les manques de données DSI et leur horodatage. Répéter profilage désactivé pour évaluer son coût.
2. Si un flash survient **sans publication UI** mais avec augmentation des manques DSI, rechercher la contention mémoire, l'alimentation et le pilote. Le scan SD inutile de l'ancien accueil peut y contribuer, mais les tests hôte ne démontrent pas ce lien électrique ou temporel.
3. Si le flash correspond à une grande invalidation, relever le service attribué et l'heure, puis reproduire avec les mêmes options et la même SD. Ne pas remplacer l'enquête par une réinitialisation périodique ou supprimer des frames.
4. Pour le lag de souris, distinguer l'intervalle de traitement des entrées, la durée active UI et la cadence effectivement visible. Le modèle DMA hôte ne constitue pas une mesure de la latence de l'écran réel.

**Aucun essai sur Tab5 physique n'a été réalisé.** La disparition des flashs au repos reste un critère matériel à vérifier.
