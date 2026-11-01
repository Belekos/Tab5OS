# Tab5OS 0.8.47

Refonte ciblée des infrastructures communes à l'affichage, Média et l'audio, à partir de la 0.8.46 réellement livrée.

- Transfert RGB565 par blocs vers l'image arrière DSI, avec rotations et pas de ligne vérifiés ; repli M5GFX conservé.
- Construction transactionnelle des listes Média, conservation des lignes visibles, styles/libellés actualisés seulement si nécessaire ; correction des onglets exclusifs.
- Tri des noms dans une arène PSRAM, sans une allocation par élément.
- Préférences Média confiées à une tâche temporaire, sans écriture NVS depuis les actions graphiques ; compatibilité des données existantes et traitement des erreurs conservés.
- Démarrage du codec après établissement des horloges et transfert DMA de silence, avec sortie muette pendant la configuration. Correctif M5Unified isolé et reproductible.
- Mutex natif à héritage de priorité pour l'admission mémoire et événements de fin de collecte pour les workers.
- Suspension de l'indexation structurelle des documents cachés ; récupération sous pression des seuls index syntaxiques reconstruisibles sans arrêt fatal. Documents, historique, sélection et autosauvegarde inchangés.
- Suppression de cinq anciens callbacks Média sans appelant, et diagnostics ciblés des transferts et écritures.

Les tests hôte et la compilation ne prouvent pas la disparition des flashs ou claquements sur la tablette. Consulter `ARCHITECTURE_0.8.47.md`, `AUDIT_0.8.47.md`, `BILAN_RESSOURCES_0.8.47.md` et `TESTS_TAB5_0.8.47.md`. Aucune partition, clé ou donnée utilisateur n'est effacée par cette mise à jour.
