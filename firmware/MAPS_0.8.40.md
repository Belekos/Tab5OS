# Cartes — recherche locale et repères, 0.8.40

La recherche commence sur la carte SD, même sans Wi-Fi. Elle réunit les lieux enregistrés, les positions Meshtastic lorsque leur affichage est activé, et les localités GeoNames. L’action distincte de recherche en ligne conserve l’accès à Nominatim pour une recherche plus large. Une carte de fond hors ligne nécessite toujours des tuiles déjà mises en cache ou un paquet importé ; l’index des noms ne contient pas les images des cartes.

## Couverture des noms

L’index livré contient **171 590 localités** et **980 448 clés** (noms, variantes et certains suffixes de noms composés), extraits de GeoNames `cities1000.zip`. Ce jeu comprend les lieux peuplés de plus de 1 000 habitants et les sièges administratifs jusqu’au niveau PPLA3 selon la source. Il ne fournit pas toutes les rues, adresses, petits hameaux ou tous les points d’intérêt.

Les préfixes sont reconnus sans distinction de casse ni d’accents latins courants. Les variantes présentes dans GeoNames permettent, par exemple, « Londres », « München » et « 東京 ». Le classement favorise les noms exacts, puis les favoris/points Mesh, puis la proximité du centre de la carte. La population départage les localités dans une même bande de proximité d’environ 55 km. Huit résultats au maximum sont renvoyés, accompagnés du pays et des codes administratifs disponibles. Les coordonnées sont WGS84, dans les limites de latitude Mercator. Une recherche très générale est bornée à 50 000 clés ; l’interface peut inviter à préciser la saisie.

Les favoris existants de `/System/map_places.json` restent les repères personnels, avec leur nom et leur position. Un résultat sélectionné reste marqué sur la carte. Les points Mesh sont issus des positions réellement reçues ; ils sont recherchables par nom ou identifiant hexadécimal et leur affichage suit l’option Meshtastic. L’ouverture depuis un message est intégrée par l’application principale. Le moteur expose le nom, le type, l’identifiant Mesh et l’horodatage pour l’affichage.

## Stockage et mémoire

`/System/maps/gazetteer/cities.t5geo` occupe **37 831 158 octets** sur SD. Il n’est ni chargé en RAM ni intégré au firmware. L’ouverture de l’application et le démarrage de l’OS ne parcourent pas cette base. Une requête ouvre le fichier dans le worker Cartes existant, effectue une recherche dichotomique, puis lit uniquement les entrées du préfixe demandé. Le tampon SD est limité à **4 Kio en PSRAM**, les résultats à huit entrées. La lecture des clés après la recherche dichotomique est séquentielle.

Les favoris et positions Mesh sont copiés dans un instantané borné en PSRAM avant l’envoi au worker ; il n’y a aucune lecture concurrente des tableaux modifiés par l’interface. Les remplacements, annulations, erreurs et fermetures libèrent instantanés, résultats et tampon. Les lectures détiennent un bail de stockage, fermé avant tout HTTP ; la récupération de la SD peut donc attendre les utilisateurs actifs. La recherche locale passe devant une tuile différée par la priorité réseau audio. Les annulations sont vérifiées pendant la dichotomie, toutes les 64 clés et avant les métadonnées de résultat.

Les révisions de repères changent seulement après une modification des lieux, une sélection de résultat, un changement de positions Mesh ou de leur visibilité. Le rendu de repères et son intégration LVGL sont vérifiés séparément par les tests de l’interface principale. Les repères restent une couche visuelle ; les exports raster historiques de carte ne les incluent pas tant que cette couche n’est pas explicitement composée par l’export.

## Provenance et reconstruction

- Source officielle : <https://download.geonames.org/export/dump/cities1000.zip>.
- Format, champs et couverture : <https://download.geonames.org/export/dump/readme.txt>.
- Attribution : **GeoNames**, <https://www.geonames.org/>.
- Licence des données : **CC BY 4.0**, <https://creativecommons.org/licenses/by/4.0/>.
- L’index est une conversion réalisée pour Tab5OS ; les noms et la couverture suivent la source, qui peut contenir des erreurs ou omissions.

`cities.json`, placé avec l’index et son `README.txt`, enregistre l’URL, la date de conversion, les nombres d’enregistrements, la version Unicode et les SHA-256 de l’archive source, du générateur, de la table de normalisation et de l’index. Le ZIP source utilisé a pour SHA-256 `5e1bf5a85957fd65602dd66fb02d3bdcffd3eaa377204769c5035487f57d95bc` ; l’index livré a pour SHA-256 `681a41de79c9e6d2374f5f8d704790f8b54ed4fb72abdf371c212f59ed1cf18d`.

```sh
python3 tools/maps-gazetteer/build.py cities1000.zip \
  Tab5OS_Arduino_v0.8.40_Ecriture/sdcard/System/maps/gazetteer/cities.t5geo \
  --fold-header Tab5OS_Arduino_v0.8.40_Ecriture/Tab5GeoFold.h
```

Le générateur ne télécharge rien. Remplacer l’index SD lorsque l’OS est arrêté ou le support éjecté. Toute modification du générateur ou de la normalisation impose de reconstruire ensemble l’index et `Tab5GeoFold.h`.

## Vérification reproductible

```sh
python3 tools/maps-tests-0840/run.py --build /tmp/maps-host --final
python3 tools/maps-tests-0840/generator.py --build /tmp/maps-generator \
  --source cities1000.zip --final
python3 tools/maps-tests-0840/target.py --sdk-root /path/to/build_0840 \
  --build /tmp/maps-p4 --final
```

Les rapports `tools/verification/maps-0840.json`, `maps-generator-0840.json` et `maps-target-0840.json` lient chaque résultat aux empreintes des sources. Les tests hôtes ASan/UBSan exécutent le cœur de recherche directement et les méthodes de requête/worker/repères extraites textuellement du code de production. Seules les frontières FreeRTOS, réseau, fichier et API Mesh sont adaptées à l’hôte ; la base livrée est réellement lue. Ils couvrent noms multilingues, classement, coordonnées, entrées corrompues, E/S en erreur, absence de SD, concurrence de remplacement/annulation, quinze cycles de fermeture/réouverture et échecs d’allocation. Les allocations PSRAM suivies doivent revenir à zéro. Les 18 455 cas de normalisation comparent le générateur Python et le code C++ ; la reconstruction depuis l’archive officielle doit reproduire exactement le SHA-256 de l’index.

La compilation isolée du module utilise le GCC officiel ESP32-P4 et les en-têtes M5Stack/Arduino réels. Les tailles de pile fournies sont des cadres statiques de fonctions, pas une mesure complète du niveau d’eau de la tâche. Aucun délai SD, radio Meshtastic ou comportement matériel n’est présenté comme mesuré sur une tablette physique.
