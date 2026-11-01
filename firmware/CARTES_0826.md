# Cartes · 0.8.26

Le bouton **i** ouvre les informations du centre de la carte immédiatement. La touche **I** conserve cette action. Un appui long sans déplacement sur la carte ouvre les informations du point touché. Un déplacement du doigt conserve le panoramique. Les actions Centrer ici et Enregistrer ce lieu restent disponibles.

La recherche conserve ses huit résultats, les noms traduits et les alias. Elle accepte jusqu'à 96 caractères, y compris les caractères UTF-8 sur quatre octets. Une recherche remplacée ou abandonnée en quittant Cartes est annulée ; sa réponse ne doit pas réapparaître après une réouverture.

Le téléchargement réutilise les objets HTTP/TLS déjà possédés par le worker Cartes. La connexion est fermée avant le décodage JSON. Le décodeur conserve seulement les coordonnées, le nom affiché, l'alias et les traductions utiles à la langue courante. Les coordonnées invalides, non finies ou hors limites sont écartées. Les allocations restent en PSRAM, sans repli dans la RAM interne réservée aux pilotes et au réseau.

La mémorisation optionnelle de la dernière position et du zoom reste effectuée à la sortie de Cartes ; aucun enregistrement périodique sur microSD n'est ajouté.

## Vérification et limites

- Test du décodeur de production avec ArduinoJson 7.4.3 et AddressSanitizer : 9 615 vérifications, 300 cycles et 24 positions d'échec d'allocation nouvelle ou croissante. Le contrat d'ArduinoJson impose qu'un rétrécissement d'allocation réussisse ; le faux allocateur respecte ce contrat.
- Pour un exemple contenant 800 noms traduits inutilisés, pic d'allocation JSON mesuré sur l'hôte : **8 586 octets**, contre **179 430 octets** pour l'ancien document complet. Ces nombres concernent ce jeu de données et ce compilateur hôte, pas une mesure de consommation totale sur tablette.
- Interface réelle LVGL 9.6, contrôleurs extraits des sources de production, Focus/TextInput et cœur d'événements sous AddressSanitizer : 1 206 vérifications, dont 200 ouvertures/actions/fermetures, les vrais événements d'appui long et de déplacement du pointeur, et la touche I. La même assertion d'ouverture immédiate du bouton i échoue avec le contrôleur .25.
- Compilation de `Tab5Map.cpp` avec le vrai SDK ESP32-P4. Frame de `downloadSearch` : 240 octets contre 768 en .25 ; frame du worker : 928 contre 656, car la requête accepte désormais 96 caractères UTF-8 sans troncature. Il s'agit de frames individuelles, pas d'une borne complète de la chaîne TLS.

Le reset matériel signalé lors d'une recherche n'a pas été reproduit sur une tablette dans cet environnement. Cette version supprime les allocations redondantes et réduit le document JSON, ajoute l'annulation et la validation, et journalise `[STACK] map-search-before` / `map-search-after` pour mesurer la marge de pile pendant une vraie recherche. Elle ne prétend pas identifier une cause matérielle unique sans trace de reset.

Rapports reproductibles : `tools/verification/maps-search-0826.json` et `maps-target-0826.json`. Bancs : `tools/maps-tests-0826/run.py` et `target.py`. Les frontières Wi-Fi, TLS et microSD ne sont pas simulées par le banc d'interface.
