# Tests Média 0.8.44

Depuis la racine du projet :

```sh
python3 tools/media-ui-tests-0844/run.py
python3 tools/media-ui-tests-0844/list.py
```

Ils utilisent LVGL 9.6 réel et son archive instrumentée présents dans `build_0842/host-dependencies/lvgl-9.6.0` et `host_lvgl_0842` du workspace. Modifier uniquement ces chemins dans les scripts pour un autre environnement ; compiler l'archive avec `lv_conf.h`, ASan et UBSan. Aucun exécutable hôte n'est inclus dans le projet.

`run.py` extrait les cinq fonctions actuelles de mise à jour du sketch et les structures audio/vidéo, puis les lie à LVGL réel. Les moteurs, la session et l'horloge sont des frontières contrôlées. La méthode de notification audio est un adaptateur, **pas le producteur réel** : elle teste la réaction de l'interface aux événements, aux échecs et à leur ordre. Le producteur réel est testé dans les tests audio de la même version. `baseline/` contient uniquement les fonctions et structures extraites de 0.8.43 et leur provenance, permettant la comparaison sans ancien projet externe.

`list.py` compile le code complet réel du modèle de liste, du focus, de la saisie, du style et du document. Le portage du test historique couvre 800 références, clavier/souris/tri, favoris, clics périmés et destruction. Les tests utilisent ASan/UBSan ; LeakSanitizer est désactivé dans cet exécuteur. Ils n'exercent aucun périphérique physique et ne démontrent pas la disparition des flashs MIPI-DSI.
