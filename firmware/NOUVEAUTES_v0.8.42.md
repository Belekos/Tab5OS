# Tab5OS 0.8.42 — stabilisation de la livraison 0.8.41

Cette version conserve les applications et les comportements de navigation, d’enregistrement et d’autosauvegarde. Elle vise les causes démontrées de blocage et les erreurs de récupération des données ; aucun résultat matériel n’est revendiqué sans essai sur Tab5.

| Module | Correction | Effet attendu et limite |
|---|---|---|
| Affichage et souris | La barrière DSI attend la présentation sans immobiliser la tâche graphique ; une nouvelle image reste différée tant que le précédent framebuffer n’est pas libéré. Tick LVGL fourni par `millis`. | Entrées et services peuvent progresser pendant l’attente. Les flashs turquoise et la cadence physique restent à mesurer. |
| Applications ELF | Lecture, validation et relocation sur worker ; publication de l’erreur une seule fois ; annulation/fin et libération ordonnées. Affichage natif : verrou de trame non bloquant côté UI, entrées séparées. | Le chargement ou une copie de pixels ne retiennent plus le service UI. L’ELF partage toujours la mémoire du firmware. |
| Vidéo | Fermeture coopérative et reprise de la dernière demande via `service()`, sans boucle d’attente dans un callback UI. | Interface disponible pendant la fin du worker ; image libérée seulement après détachement et fin effective. |
| Audio | Les accesseurs scalaires ne construisent plus un snapshot contenant six chaînes. | Suppression d’allocations/copies superflues à chaque consultation d’état ; qualité audio à tester sur appareil. |
| Mémoire | Admission commence par les octets libres, puis parcourt uniquement les catégories dont un bloc contigu est demandé. | Moins de parcours pendant une pénurie ; les marges et contraintes restent vérifiées. La PSRAM ne remplace pas les piles/DMA internes. |
| Stockage et secrets | Une erreur de lecture ou de présence n’est plus assimilée à une absence. Écritures temporaires et récupération du précédent ne détruisent pas une version valide sur panne. État de déverrouillage publié sans attendre une opération cryptographique. | Préserve paramètres/documents/secrets face aux fautes testées. FAT et alimentation restent des limites matérielles. |
| Réparation des ressources | Les 40 fichiers de secours sont compressés avec zlib déjà liée et restaurés en étapes bornées en PSRAM. | Marge OTA accrue sans retirer de ressources ; aucune nouvelle écriture au démarrage, aucun remplacement des fichiers déjà présents. |
| Diagnostics | Profilage UI optionnel, huit événements lents bornés, contexte RTC selon reset ; relevés des tas et piles à la demande. | Permet d’identifier un service lent ou un besoin de mémoire interne. Temps écoulé, pas pourcentage CPU ; aucune écriture permanente Flash/SD. |

Les quatre nouveaux libellés sont traduits dans les quatorze packs ; toutes les traductions précédentes sont conservées. La charte graphique, les noms anglais des nouveaux fichiers, les ressources Factory, l’OTA et la table de partitions sont conservés.

Le [diagnostic détaillé](AUDIT_0.8.42.md), le [compte rendu de validation](VERIFICATION.md), le [bilan des ressources](BILAN_RESSOURCES_0.8.42.md) et les [essais matériels](TESTS_TAB5_0.8.42.md) séparent les observations établies des points restant à confirmer. Les dossiers `tools/audit-0842` et `tools/verification` contiennent les analyses et preuves par module. Les notes 0.8.41 et antérieures restent historiques.
