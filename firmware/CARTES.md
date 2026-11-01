# Cartes : consultation et préparation hors ligne

En 0.8.45, la recherche privilégie Internet lorsque la connexion et le transport sont disponibles, puis les données locales après échec ou résultat vide. Sans Wi-Fi ou pendant le bail réseau exclusif d'une radio, la recherche locale reste directement disponible.

Le zoom et le déplacement restent possibles pendant le chargement des tuiles. Les requêtes de l'ancienne vue sont abandonnées pour laisser charger la nouvelle vue.

Le curseur propose maintenant un zoom d'affichage jusqu'à **22** pour les sources
fournies. Pendant un déplacement du curseur, seul le niveau annoncé change ; le
chargement commence au relâchement. Les flèches du clavier sur le curseur appliquent
également le changement. Dans la carte : flèches pour déplacer, `+`/`−` pour zoomer,
`Début` pour revenir au lieu de départ, `I` pour les informations au centre,
`Échap` pour revenir aux commandes.

## Position au démarrage et reprise

Sur une tablette sans préférences Cartes, la vue initiale est mondiale et neutre :
latitude 0°, longitude 0°, zoom 2. Aucune position personnelle n’est fournie.

Dans **Cartes → Réglages de la carte** (icône de carte), l’interrupteur
**Mémoriser la dernière vue** est activé par défaut. La position et le zoom
d’affichage sont enregistrés lorsqu’on quitte Cartes pour une autre page. Ils sont
restaurés à la prochaine ouverture, y compris après redémarrage. Le zoom reste
le zoom d’affichage (jusqu’à 22) ; le moteur choisit toujours le niveau de tuiles
effectivement disponible chez la source.

Lorsque cette option est désactivée, ouvrir Cartes revient à la position
d’accueil explicitement enregistrée dans **Lieux enregistrés**, ou à la vue
mondiale si aucun accueil n’a été défini. Réactiver l’option enregistre la vue
courante à la prochaine sortie. Définir un accueil ne remplace pas la dernière
vue mémorisée. Les anciennes positions enregistrées sur la tablette sont lues
si elles sont complètes et valides ; aucune donnée de position n’est importée
du PC qui compile le firmware.

La reprise utilise un unique enregistrement de **24 octets dans la mémoire
interne NVS**, indépendamment de la microSD. Aucune sauvegarde n’est faite pendant
les déplacements, le zoom ou un rafraîchissement. Quitter et rouvrir sans changer
la vue ne réécrit pas cet enregistrement. Une valeur illisible, incomplète ou
hors limites est ignorée ; un échec de sauvegarde est signalé à l’écran.

Une coupure forcée pendant que Cartes reste ouverte ne peut pas enregistrer le
dernier déplacement : la prochaine ouverture reprend la dernière sortie
réussie. Quitter Cartes avant d’éteindre conserve la vue affichée.

## Zoom affiché et détails disponibles

| Source fournie | Dernier zoom des tuiles | Zoom d'affichage maximal | Agrandissement au maximum |
| --- | ---: | ---: | ---: |
| OpenStreetMap | 19 | 22 | ×8 |
| OSM léger | 19 | 22 | ×8 |
| OpenTopoMap | 17 | 22 | ×32 |

Au-delà du dernier niveau de la source, la carte agrandit les tuiles parentes.
Cela facilite la lecture sans inventer de nouveaux détails. OSM léger continue à
utiliser un niveau inférieur lorsque c'est possible. L'agrandissement est borné
à cinq niveaux ; il ne crée pas une image géante en mémoire : le rendu calcule
uniquement les pixels nécessaires dans le canevas existant.

Le zoom est conservé avec la dernière vue, le domicile et les lieux enregistrés.
Les paquets `.t5map` et les caches XYZ existants restent compatibles.

## Informations sur un point

Activer le bouton **ⓘ**, puis toucher la carte sans la faire glisser. La fenêtre
indique latitude/longitude, zoom affiché, zoom de la source, échelle approximative
en mètres par pixel, source et nombre de paquets locaux pour cette source. Un lieu
enregistré apparaît s'il se trouve à moins de 48 pixels du point choisi.

La fenêtre permet de centrer la carte ou d'enregistrer ce lieu. `I` ouvre les
informations du centre au clavier ; les actions se parcourent avec `Tab` et
`Échap` ferme la fenêtre. L'activation du mode information n'empêche pas de faire
glisser la carte.

Cette consultation n'envoie aucune requête. Les tuiles raster ne contiennent pas
de fiches de bâtiments, d'adresses ou de commerces : ces informations ne sont pas
déduites de l'image. Le nombre de paquets correspond à la source, sans garantir
que chaque paquet couvre le point choisi.

## Préparer une zone sur PC ou sur la tablette

Les deux importeurs acceptent maintenant une plage de zooms d'affichage **0–22**.
Ils la convertissent vers les niveaux effectivement disponibles avant d'estimer,
télécharger ou construire les paquets. Par exemple, une zone demandée seulement
à z22 importe ses tuiles z19 pour OSM, ou z17 pour OpenTopoMap. Le lecteur les
agrandit exactement de la même façon en ligne et hors ligne.

Sur la tablette, cadrer la zone puis toucher l’icône de téléchargement : la
fenêtre native **Télécharger la zone** contient la source, les deux zooms et
la fréquence des requêtes. Aucun programme MicroPython n’est nécessaire.
Le rectangle visible à l’ouverture reste le périmètre de l’import. Le nombre
de tuiles est calculé immédiatement ; au-delà de 50 000 tuiles, réduire les
détails ou cadrer une zone plus petite. Sur PC, la zone reste un cercle défini
par son centre et son rayon. L’outil PC est facultatif.

### Fréquence des requêtes

- **PC** : `tools/Cartes_Tab5.cmd`, encadré **Vitesse de téléchargement** puis
  champ « Requêtes par seconde », conservé entre
  les ouvertures. Plage : **0,1 à 100 requêtes/s**, plafond commun à toutes les
  connexions. En ligne de commande : `--requests-per-second 0.5`.
- **Tablette** : Cartes → icône téléchargement → curseur **Requêtes par seconde**.
  Plage : **0,1 à 4** ; la valeur est conservée dans les préférences de l’appareil.
  La limite réellement appliquée par la source est affichée à côté du curseur.
- **OpenTopoMap public** : plafond conservé à **0,5 requête/s**, soit une requête
  toutes les deux secondes au minimum, même si une valeur supérieure est saisie.
  Il est toujours possible de choisir une fréquence inférieure.

Il s'agit d'une fréquence maximale, pas d'une vitesse garantie. Le temps réseau
s'ajoute aux délais ; le serveur et la connexion peuvent ralentir l'import.
Mettre l’import natif en pause pour modifier sa cadence, puis reprendre. Le
réglage est enregistré au relâchement du curseur ou lors d’une modification au clavier.

Les réessais du PC respectent également le délai configuré. L’import natif se
met en pause en cas d’erreur et demande une reprise explicite : un refus 403 ou
429 n’est jamais suivi d’un nouvel essai automatique.
Les temporisations sont annulables et l'interface reste disponible. Pendant un
transfert déjà engagé, Pause est prise en compte à sa fin ou à l'expiration de
son délai réseau. Sur PC, Pause ne tue plus le processus d'import de tuiles.

Chaque nouvelle tuile est vérifiée puis publiée dans le cache avant d’avancer.
Une panne de transfert conserve les fichiers déjà terminés. Pause, Fermer et
Échap annulent les requêtes restantes. Pour reprendre, rouvrir la même zone et
les mêmes zooms : les tuiles du cache sont sautées. La tablette garde au plus
une tuile en attente, de 512 Kio maximum, et 23 rectangles de coordonnées ;
aucune liste proportionnelle au nombre de tuiles. Les téléchargements utilisent
le worker Cartes existant, les réservations mémoire et la supervision réseau de
l’OS. L’ancienne application Python d’import reste utilisable séparément.

## Sources et conditions d'utilisation

Le serveur standard `tile.openstreetmap.org` est réservé à la consultation
interactive et interdit le préchargement de zones. Les importeurs conservent ce
blocage. Pour OSM hors ligne, utiliser le rendu local PC fourni ou une source qui
autorise cet usage. Une fréquence personnalisée ne retire pas les conditions de
la source ni les limites de l'application.

- Politique OSM : <https://operations.osmfoundation.org/policies/tiles/>
- Source, attribution et état du service OpenTopoMap : <https://opentopomap.org/about>
- Guide PC complet : `Tab5OS_Arduino_v0.8.28_Ecriture/tools/CARTES_PC.md`.

## Vérifications de la version 0.8.23

`tools/maps-tests/run.py` exécute les contrôleurs de zoom extraits du programme,
avec LVGL réel, la géométrie et le rendu raster de production. Les importeurs PC
et tablette sont exécutés avec un réseau simulé : correspondance des tuiles,
limites, reprise, fréquence, erreur HTTP 429 et annulation. Le résultat est
`tools/verification/maps-0823.json`. Les nouveaux tests exécutent aussi la fenêtre
native complète avec LVGL, 200 ouvertures/imports/reprises/fermetures, les limites
des sources, le changement de cadence, le passage de l’antiméridien, l’arrêt sur
erreur et le bouton d’accueil des lieux enregistrés à 20, 26 et 32 pixels.
Les fermetures passent par le vrai bouton et la touche Entrée du gestionnaire
de focus de l'OS, ainsi que par le chemin Échap. AddressSanitizer surveille le
contrôleur natif, le focus et les événements LVGL pendant ces cycles.
Le contrôleur PC est exécuté avec uniquement ses widgets Tk simulés.

Ces tests ne mesurent pas les performances du Wi-Fi, de la carte SD ou de l'écran
sur une tablette physique.

## Informations et recherche en 0.8.26

Le bouton i affiche les informations du centre ; un appui long sur la carte choisit un point. La recherche conserve les huit résultats, filtre les données JSON utiles et annule les requêtes abandonnées. Les allocations et contrôles logiciels sont décrits dans [CARTES_0826.md](CARTES_0826.md) ; la disparition du redémarrage doit être confirmée sur tablette.
