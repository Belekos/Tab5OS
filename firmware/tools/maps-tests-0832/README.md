# Cartes / saisie 0.8.32 — régression hôte

```sh
python3 tools/maps-tests-0832/run.py --libraries /chemin/Arduino/libraries
```

Dépendances : GCC/G++, ar, Python 3 ; LVGL 9.6.0 et ArduinoJson 7.4.3. Aucun téléchargement ni changement des bibliothèques installées. Les objets sont compilés hors du projet ; `--build` conserve ce répertoire entre exécutions.

La recherche passait par la composition de saisie, même avec un clavier français. En 0.8.30, `imeInput` conservait le champ après sa destruction. La boucle `imeService()` relisait ce pointeur libéré via `lv_obj_is_valid()`, qui déréférence l’objet dans LVGL 9.6. Le correctif suit `LV_EVENT_DELETE`, détache le précédent champ lors d’un changement de cible et libère la composition et la page de candidats. L’échec d’enregistrement du suivi n’autorise pas la conservation d’un pointeur non suivi.

Le harnais compile l’intégralité de `Tab5ImeUi.inc`, le vrai compositeur, la vraie logique numérique, Focus/TextInput/WriterDocument et LVGL/TinyTTF avec la police SD. Les autres applications et le dictionnaire SD sont des adaptateurs hôtes. Il exécute 400 cycles de champs de recherche en mode latin (saisie, validation, destruction, service, réouverture), 300 cycles de changement/suppression de champs chinois, japonais ou coréens, l’invalidation des références aux panneaux et l’échec simulé d’allocation du suivi d’objet. Le source 0.8.30 exact `Tab5ImeUi-0830.inc.txt` reproduit l’accès libéré sous ASan ; son échec est attendu et journalisé.

Un second harnais extrait exactement `requestSearch`, `pollSearch`, `releaseSearch`, `taskLoop` et `taskEntry` du moteur. Des files, mutex et horloges hôtes remplacent FreeRTOS ; les téléchargements sont scénarisés. Il couvre 400 recherches successives, 40 annulations en cours avec remplacement, 40 arrêts/redémarrages du worker et la restitution de chaque tableau de résultats. Il ne remplace pas un essai réseau réel.

Les contrôleurs des résultats et l’encodeur URL sont extraits exactement du sketch. Les régressions de la 0.8.30 restent couvertes : Entrée/Espace, callbacks destructifs, conservation du focus sur une liste survivante, JSON filtré et échecs d’allocation. L’espace est toujours transmis comme `%20`.

Tous les tests corrigés s’exécutent avec AddressSanitizer et UndefinedBehaviorSanitizer. LeakSanitizer est désactivé car l’hôte ne permet pas son inspection des processus ; le harnais du worker compte explicitement les tableaux de résultats vivants. Les allocations LVGL utilisent l’hôte à la place de la PSRAM ; le correctif arabe LVGL déjà livré est appliqué uniquement à l’objet compilé.

Le rapport `tools/verification/maps-0832.json` lie les résultats aux sources par SHA256. Aucun essai de tablette, HTTP/TLS, C6, service de géocodage réel ou stockage SD matériel n’est revendiqué. Le défaut logiciel est reproduit ; la correspondance exacte avec l’incident sur la tablette reste à confirmer.
