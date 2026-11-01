# OpenStreetMap et OpenTopoMap pour Tab5OS

Ce programme prépare une zone complète sur le PC puis la copie sur la carte SD. Il propose uniquement **OpenStreetMap** et **OpenTopoMap**, avec leurs identifiants de cache reconnus par Tab5OS.

## Démarrage Windows

Installer Python 3.10 ou plus récent avec Tkinter (inclus dans l’installateur Windows officiel), puis ouvrir **`Cartes_Tab5.cmd`**. Aucun paquet Python supplémentaire n’est nécessaire.

1. Choisir **OpenTopoMap** pour télécharger directement, sans compte ni Docker. C’est le choix initial ; une préférence déjà enregistrée reste conservée.
2. Rechercher une ville et sélectionner le bon résultat, ou saisir latitude/longitude.
3. Choisir le rayon, par exemple **100 km**, puis les zooms. Commencer par **0 à 12** pour évaluer le volume.
4. Choisir un dossier PC et, facultativement, la racine de la carte SD.
5. Dans **Vitesse de téléchargement**, régler **Requêtes par seconde** (0,1–100).
   La cadence effective et la limite de la source s’affichent immédiatement.
   Le choix est conservé entre les ouvertures.
6. Cliquer **Estimer**, puis **Télécharger / reprendre**.

Cet outil PC est facultatif. Sur la tablette, l’icône de téléchargement de
Cartes ouvre directement un import natif de la zone affichée, avec son propre
curseur de fréquence (0,1–4 requêtes/s). Il n’ouvre pas une application Python.

Le calcul couvre les tuiles intersectant le cercle, y compris près de l’antiméridien. Il ne se limite pas aux tuiles actuellement visibles. Chaque niveau de zoom supplémentaire peut multiplier par environ quatre les tuiles détaillées. L’estimation utilise 32 Kio par tuile, mais le contenu de chaque PNG varie. Le PC conserve à la fois les PNG du cache et les paquets, soit environ deux fois le volume annoncé pour les cartes. La carte SD reçoit uniquement les paquets.

**Pause** garde les PNG terminés. Relancer exactement la même zone reprend ces fichiers après vérification. Changer de source ou de zone exige un nouveau nom de paquet pour éviter de mélanger les données. Un téléchargement incomplet n’est jamais annoncé comme une zone terminée.

Les paquets sont publiés après écriture complète et sont découpés en volumes de **1 900 Mio maximum par défaut** (limite configurable : 2 000 Mio). Cette taille permet de lire entièrement chaque fichier avec le firmware actuel ; la capacité totale de la carte SD peut être bien supérieure. Il peut donc y avoir plusieurs fichiers `.t5map`. Tous sont nécessaires. Tab5OS lit jusqu’à **128 paquets dans ce dossier, toutes cartes confondues**. Le programme refuse une zone qui dépasserait cette limite et vérifie le nombre de paquets avant la copie SD ; les anciens volumes remplacés et inchangés sont pris en compte dans ce calcul. Conserver aussi le fichier `.t5map.manifest.json` généré : un export ultérieur l’utilise pour retirer uniquement ses anciens volumes inchangés. Les fichiers personnels, modifiés ou anciens sans manifeste restent conservés. Si la SD n’était pas sélectionnée, copier les volumes et le manifeste dans :

```
System/maps/offline/
```

Si un ancien paquet dépasse 2 Gio, le reconstruire depuis le cache PC avec cette version de `build_t5map.py`, ou relancer la même commande de téléchargement : les PNG terminés seront réutilisés sans les télécharger à nouveau. Le découpage inclut les données et leurs index.

Sur la Tab5, ouvrir **Cartes** et choisir **OSM Standard** ou **OpenTopoMap**. Les paquets sont lus directement ; aucune conversion ni base SQLite n’est chargée en RAM. Conserver le cache PC si l’on souhaite reprendre ou reconstruire les paquets. Le supprimer ensuite ne retire pas les cartes de la SD.

## OpenTopoMap : source réelle, débit modéré

Laisser **URL XYZ** vide : le programme utilise directement `https://a.tile.opentopomap.org/{z}/{x}/{y}.png`, jusqu’au zoom 17. Une URL personnelle reste associée à la carte choisie ; passer d’OSM à OpenTopoMap ne réutilise plus accidentellement l’adresse du serveur OSM. Le projet autorise l’intégration de sa carte à condition de ne pas surcharger son serveur, notamment par des téléchargements massifs. Il ne publie pas de quota garantissant qu’une zone de taille quelconque sera acceptée.

Le programme impose donc **une seule connexion et au plus une tuile toutes les deux secondes** sur ce serveur public. Il affiche la durée minimale correspondante avant le téléchargement ; le temps de réponse du serveur s’ajoute à ce minimum. Par exemple, Paris avec 100 km de rayon et les zooms 0 à 14 représente 16 735 tuiles, environ 523 Mio à 32 Kio/tuile et un minimum de 9 h 18 min hors cache. Une zone détaillée peut prendre plusieurs jours. En cas de refus HTTP 403 ou 429, il s’arrête et conserve la reprise ; il ne change pas de sous-domaine pour contourner ce refus. Ce débit prudent n’est pas une permission de téléchargement illimité, ni une garantie de disponibilité.

Pour de très grandes zones, renseigner un serveur personnel ou explicitement autorisé dans le champ URL. Le [projet OpenTopoMap](https://github.com/der-stefan/OpenTopoMap) publie son style et les instructions pour produire ses propres tuiles ; ce rendu topographique nécessite aussi les données de relief et est plus complexe que l’installation OSM fournie ici. Les fichiers Garmin ne sont pas des PNG et ne sont pas directement compatibles avec Tab5OS.

Le projet OpenTopoMap annonce une transition future vers des tuiles vectorielles. Ce programme utilise les PNG raster encore servis ; les futurs fichiers vectoriels ne seront pas directement lisibles par Tab5OS. Les paquets déjà copiés sur la SD restent utilisables hors ligne.

L’attribution reste visible sur la tablette : données © OpenStreetMap, SRTM ; carte © OpenTopoMap, CC-BY-SA.

## OpenStreetMap : le même fond, calculé sur le PC

Le serveur public `tile.openstreetmap.org` interdit la préparation de zones hors ligne. Ce point dépend du serveur, pas du fait d’utiliser une tablette ou un PC. Les données OSM, elles, sont disponibles en téléchargement régional. La solution fournie utilise ces données et le véritable style **OpenStreetMap Carto** via le projet libre `overv/openstreetmap-tile-server`.

Installer et démarrer **Docker Desktop avec les conteneurs Linux**, puis choisir **Préparer OSM sur ce PC**. Sélectionner un fichier `.osm.pbf` déjà téléchargé ou son URL HTTPS depuis [Geofabrik](https://download.geofabrik.de/). Choisir une région couvrant **tout le cercle demandé**, quitte à prendre un pays lorsque la zone traverse plusieurs régions. Les routes hors de l’extrait ne peuvent pas être inventées par le moteur de rendu.

Avant l’import, le programme affiche les ressources prévues et demande confirmation : jusqu’à **12 Gio de RAM**, **4 threads**, et au moins **40 Gio de disque disponible dans le stockage Docker** pour un petit extrait ; une grande région demande davantage. L’import et les premiers rendus peuvent être longs. Le GPU n’est pas requis. Les composants complémentaires sont téléchargés lors du premier import ; le rendu local fonctionne ensuite sans Internet.

Après l’import, le serveur écoute seulement sur le PC :

```
http://127.0.0.1:8080/tile/{z}/{x}/{y}.png
```

C’est la source OSM utilisée par défaut dans le programme. Le rendu des tuiles basses peut dépasser une minute sur de grandes régions ; si nécessaire, le mode console accepte `--timeout 300`. Le fond est celui d’OpenStreetMap Carto, avec les données de l’extrait choisi ; son apparence peut légèrement varier selon la révision du style dans l’image Docker.

La préparation ne détruit jamais une base existante. Pour un nouvel extrait ou une actualisation, utiliser un nouveau nom d’instance :

```powershell
py setup_osm_server.py --pbf "C:\Cartes\region.osm.pbf" --instance region-2026-09
```

Pour voir les commandes sans rien lancer : ajouter `--plan-only`. Pour arrêter le serveur et libérer la RAM, reprendre la commande `docker stop tab5-osm-…` affichée à la fin. Les volumes restent disponibles. L’import PostgreSQL interrompu ne se reprend pas comme les PNG : le programme le signale et conserve ses volumes ; un nouvel import demande un nouveau nom d’instance.

## Mode console

```powershell
py download_t5map.py "C:\Cartes\zone-topo.t5map" --center 48.39,-4.49 --radius-km 100 --min-zoom 0 --max-zoom 12
py download_t5map.py "C:\Cartes\zone-osm.t5map" --provider osm --center 48.39,-4.49 --radius-km 100 --min-zoom 0 --max-zoom 12 --estimate-only
py download_t5map.py "C:\Cartes\zone-osm.t5map" --provider osm --center 48.39,-4.49 --radius-km 100 --min-zoom 0 --max-zoom 12 --sd-root "E:\"
py download_t5map.py "C:\Cartes\zone-topo.t5map" --provider opentopomap --center 48.39,-4.49 --radius-km 100 --min-zoom 0 --max-zoom 12
```

Sans `--provider`, le mode console utilise **OpenTopoMap**. OSM reste disponible avec `--provider osm`.

La recherche en console accepte `--city "Brest, France"`. Si plusieurs résultats sont possibles, le programme les affiche et demande de préciser `--city-index`. Il ne choisit pas silencieusement une ville homonyme. `--url-template` accepte aussi un serveur XYZ local ou autorisé ; `--header` permet l’authentification prévue par ce serveur. Le programme ne contient aucune clé commerciale.

Un ancien dossier `z/x/y.png` ou un MBTiles raster peut être converti sans nouveau téléchargement :

```powershell
py build_t5map.py "C:\AncienCache" "C:\Cartes\ancien-osm.t5map" --cache-id osm
py build_t5map.py "C:\Topo.mbtiles" "C:\Cartes\ancien-topo.t5map" --cache-id opentopomap
```

Les images doivent être des PNG de 256 × 256. Un MBTiles vectoriel n’est pas un raster et nécessite d’abord un rendu sur PC.

## En cas d’interruption

- **HTTP 403 ou 429** : arrêter et attendre ; le serveur a refusé ou limité les requêtes. Le message affiche `Retry-After` si le serveur le fournit.
- **Certificat HTTPS** : vérifier la date du PC et son installation Python ; la vérification des certificats reste active.
- **Connexion refusée à 127.0.0.1** : OpenStreetMap est sélectionné sans serveur démarré. Choisir OpenTopoMap et laisser son champ URL vide pour télécharger directement.
- **Disque plein ou dossier non inscriptible** : libérer de la place ou choisir un dossier accessible. Le cache est conservé.

Le détail affiche la tuile concernée. Reprendre la même zone et le même dossier réutilise les PNG vérifiés ; une erreur ne publie pas un paquet complet. Sur la tablette, **Cartes hors ligne → Télécharger une zone** utilise également OpenTopoMap au premier lancement, et affiche le volume et la durée minimale avant confirmation.

## Sources

- [Politique officielle des tuiles OSM](https://operations.osmfoundation.org/policies/tiles/)
- [Rendu local OSM : guide Switch2OSM](https://switch2osm.org/serving-tiles/using-a-docker-container/)
- [Moteur Docker et commandes d’import](https://github.com/Overv/openstreetmap-tile-server)
- [Usage, attribution et FAQ OpenTopoMap](https://opentopomap.org/about)


## Version 0.8.22 : zoom et fréquence

Les champs de zoom acceptent 0–22 (niveau affiché sur la tablette). L’import télécharge les niveaux réellement disponibles : jusqu’à 19 pour OSM, 17 pour OpenTopoMap ; les niveaux supérieurs affichent les tuiles parentes agrandies. L’estimation indique les deux plages. Les paquets existants restent compatibles.

Le champ **Requêtes par seconde** conserve votre choix entre 0,1 et 100. Le plafond est global pour les connexions, et OpenTopoMap public reste limité à 0,5 requête/s. En ligne de commande, ajouter --requests-per-second 0.5. Le bouton Pause laisse finir la requête en cours et conserve les tuiles terminées. Le guide ../../CARTES.md décrit aussi le mode information et le réglage sur la tablette.
