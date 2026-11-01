# Test indépendant du survol partagé 0.8.52

Ce banc exécute les vrais modules `Tab5Mouse`, `Tab5Focus`, `Tab5UiStyle`, `Tab5TextInput` et `WriterDocument`, liés à LVGL 9.6 réel avec ASan/UBSan. Seule la frontière de transport USB est simulée : les rapports relatifs entrent par `Tab5Mouse::service`, puis par le vrai indev LVGL. Le vrai constructeur `homeApplicationCard` et ses helpers sont extraits sans modification de `Tab5HomeUi.h` et exécutés dans une scène minimale rendue en pixels RGB888. Le catalogue de deux entrées, les fonctions palette/police et le lancement compteur sont les seules dépendances de ce constructeur substituées; aucun style de survol n’est ajouté par le banc.

Prérequis : g++ C++17, Python 3, sources LVGL 9.6 et bibliothèque hôte `liblvgl.a` instrumentée ASan/UBSan, avec son `lv_conf.h`. Le LVGL exécuté pour ce rapport utilisait l’allocateur CLIB, pour que l’ASan voie individuellement les objets alloués. La configuration exacte et les hachages des sources compilées sont enregistrés dans le JSON.

Depuis la racine du projet, adapter les chemins LVGL :

```sh
python3 tools/hover-review-0852/run.py \
  --sketch Tab5OS_Arduino_v0.8.52_Ecriture \
  --lvgl /chemin/lvgl-9.6.0 \
  --lvgl-build /chemin/host-lvgl \
  --build /tmp/tab5-hover-review-fixed
```

Pour reproduire le témoin, utiliser les sources authentiques 0.8.51 avec `--baseline`. Ce mode attend les deux défauts constatés (focus bouton figé et ligne native ne suivant pas le survol). Le test doit réussir dans ce mode lorsqu’ils sont reproduits; cela ne signifie pas que le témoin est corrigé.

Les vérifications incluent le choix uniquement au clic, la reprise clavier, Échap, les coordonnées après défilement à la molette, l’occultation par un overlay, les destructions d’objets survolés, l’inactivité sans rafraîchissement de liste, le heartbeat HID et les événements tactiles natifs. Les états du survol et de la sélection sont distingués avec des pixels et les callbacks `VALUE_CHANGED`/`CLICKED`.

Limites : le constructeur des cartes est exécuté, mais l’accueil complet, le catalogue SD/ELF et le lancement réel des applications ne le sont pas. Les contrôleurs Dessin/Éditeur ont des tests séparés. L’entrée clavier reproduit les deux hooks de `dispatchKeyboard`, puis appelle le vrai `Tab5Focus::key`. Le pilote matériel USB/tactile, MIPI/DSI, FreeRTOS SMP et le rendu sur Tab5 ne sont pas simulés. La détection de fuite ASan est désactivée; ce test vérifie les accès invalides et comportements exécutés, pas l’absence générale de fuite.

Rapports livrés : `tools/verification/hover-review-0852.json`, `tools/verification/home-hover-0852.json` et journaux voisins `hover-review-logs-0852/`. La preuve du défaut résiduel de style dans le constructeur .52 avant sa correction est conservée (`home-before-run.log`).
