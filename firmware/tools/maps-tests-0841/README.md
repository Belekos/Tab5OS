# Cartes 0.8.41 — molette et recherche locale

Le moteur conserve la recherche locale comme premier choix : villes/localités de l'index SD, lieux enregistrés et nœuds Meshtastic lorsque leur affichage est activé. La recherche en ligne reste une action explicite. Si le Wi-Fi est désactivé ou déconnecté lors de cette demande, pendant son attente ou pendant HTTP/TLS, le moteur utilise automatiquement les données locales sans réactiver la radio.

Les villes exigent toujours `System/maps/gazetteer/cities.t5geo` sur la SD. Un fond de carte en cache ne contient pas les noms interrogeables de toutes les villes. Un index absent/illisible laisse les favoris et les positions Mesh recherchables ; le moteur ne présente pas ces résultats limités comme une recherche mondiale complète.

La molette est traitée par `Tab5MapWheel.h`, sans allocation : un cran correspond à un niveau de zoom, le lieu sous le pointeur reste ancré. Les rapports d'une rafale sont regroupés après 60 ms de silence, avec un délai maximal de 120 ms pendant un mouvement continu. Une nouvelle génération de tuiles est demandée seulement pour un changement effectif de la vue. Un déplacement/zoom fait par une autre commande, une fenêtre modale, une sortie de Cartes ou un redimensionnement annule la molette en attente.

Les quatre files FreeRTOS de Cartes gardent leurs blocs de contrôle internes et placent leurs données en PSRAM. Elles ne sont utilisées que depuis des tâches ; aucune fonction `FromISR` ne les reçoit. Le stockage interne libéré correspond seulement aux données des files (environ 1,6 Kio selon les tailles exactes du SDK), pas à leur contrôle, aux piles ou au cache CPU. Aucun changement de taille de pile de Cartes n'est revendiqué.

## Vérification

Depuis la racine de livraison :

```sh
python3 tools/maps-tests-0841/run.py --build /tmp/tab5-maps-0841 --final
```

Les méthodes de production sont extraites sans réécriture depuis `Tab5Map.cpp` et le vrai index est lu. Seules les frontières SD, FreeRTOS, réseau et Mesh sont simulées. Les cas couvrent perte du Wi-Fi, absence/corruption/index refusé, annulation pendant une lecture, remplacement de requête, conservation de la priorité audio, OOM, création partielle des files, répétitions de session et libération des allocations suivies.

Le test de molette compile le vrai en-tête : ancrage aux quatre coins, passage de la ligne de changement de date, latitudes extrêmes, bornes des styles, invalides, retour du compteur millis, annulation sous une autre vue, rafales et absence d'allocation. Le rapport de production et ses empreintes se trouvent dans `tools/verification/maps-0841.json`.

ASan/UBSan sont activés. Le comptage d'allocation PSRAM est explicite ; LeakSanitizer est désactivé pour les bibliothèques hôtes. Ces tests ne mesurent ni latence de la carte SD physique, ni transport Bluetooth/Wi-Fi, ni fluidité tactile réelle.
