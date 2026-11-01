# Régressions vidéo 0.8.44

Depuis la racine du projet :

```sh
python3 tools/video-tests-0844/input.py --sdk /chemin/vers/m5stack/esp32/3.3.9/libraries/HTTPClient/src/HTTPClient.cpp
python3 tools/video-tests-0844/lifecycle.py
```

`input.py` reproduit d'abord le use-after-free de la 0.8.43 sous ASan (échec attendu), puis vérifie l'implémentation actuelle. `baseline_0843.txt` est une fixture historique exacte, jamais compilée dans le firmware. Le fichier SDK doit être celui de la dépendance verrouillée ; son SHA-256 est inscrit dans le rapport. Les cinq méthodes réelles de propriété/lifetime HTTP sont extraites à chaque exécution. Les frontières réseau, FreeRTOS et JPEG sont contrôlées : ce n'est pas un essai matériel ou TLS.

`lifecycle.py` vérifie les méthodes de publication, arrêt, remplacement et détachement, avec une tâche hôte contrôlée. Les binaires sont construits dans un temporaire extérieur au projet. ASan et UBSan sont actifs ; LeakSanitizer est désactivé selon les contraintes de l'environnement, et les ressources contrôlées sont comptées explicitement.

Les rapports et la trace ASan de référence sont écrits dans `tools/audit-0844`. Voir `video.md` pour les résultats et limites.
