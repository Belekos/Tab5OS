# Accueil — diagnostic et corrections 0.8.43

## Référence et reproduction

Référence : sources livrées 0.8.42. Les fichiers nécessaires au comparatif sont conservés dans `tools/home-ui-tests-0843/baseline` comme fixtures de test, hors firmware. Aucun ancien programme n’est injecté dans la production. La 0.8.43 conserve la construction des cartes, les préférences d’affichage, les actions, les icônes et la navigation existantes.

Le test `tools/home-ui-tests-0843/run.py` exécute **tout `Tab5HomeUi.h`**, le catalogue réel et les modules Focus/TextInput/WriterDocument réels avec LVGL 9.6 et ASan/UBSan. Seules les frontières matériel/FS, NVS, palette/traduction et lancement des applications sont adaptées. La boucle avance une horloge simulée par pas de 8 ms ; il ne s’agit pas d’un chronométrage ESP32-P4. Le catalogue de contrôle contient 300 fichiers sans rapport et 33 ELF ; aucune latence SD n’est inventée.

## Causes établies

1. **Parcours du stockage toutes les cinq secondes sur l’accueil natif.** Cet écran ne montre que les applications intégrées et les deux entrées ELF/Python. Pourtant, le service parcourait périodiquement les applications SD/USB et le manifeste Python, dans la tâche UI. La stabilité du catalogue évitait déjà sa republication et le redessin : cela ne supprimait pas les appels de fichiers et les traitements de découverte.
2. **Saut du défilement après modification du catalogue.** Le rendu rétablissait la position du défilement puis restaurait l’ancien focus. `Tab5Focus::focusObject()` défile vers ce contrôle ; si l’utilisateur avait fait défiler la bibliothèque à la souris en laissant un ancien focus hors écran, une actualisation remontait vers ce contrôle. Ce défaut est reproduit avec les vrais objets LVGL et le vrai gestionnaire de focus.

Le scénario d’accueil inactif produit **zéro transfert graphique après stabilisation dans les deux versions**. Il n’établit donc pas que ces scans sont la cause des flashs turquoise. Il établit du travail synchrone inutile sur le chemin d’entrée ; l’origine des flashs au repos reste à confronter aux autres services et au DSI physique.

## Changements ciblés

- L’accueil natif n’exécute plus de découverte de fichiers. Les changements de stockage sont mémorisés pour le prochain affichage d’une bibliothèque.
- L’ouverture d’une bibliothèque ELF/Python ou de la personnalisation demande une découverte. Le retour à l’accueil natif annule la demande en cours ; le catalogue publié reste disponible.
- Les bibliothèques visibles conservent la détection périodique des écritures externes n’ayant pas publié de révision, ainsi que le regroupement des notifications de fichiers. La personnalisation conserve ses règles de préférences ; aucun fichier ajouté sur la SD n’est masqué par cette optimisation.
- Lors d’une republication, le focus est retrouvé par sa clé stable et la position choisie par l’utilisateur est rétablie **après** le focus. La navigation explicite entre menus conserve son comportement.

Le catalogue 0.8.43 utilise par ailleurs un worker ponctuel sur l’autre cœur P4 pour les E/S lorsque l’admission mémoire le permet ; ce mécanisme et son repli fonctionnel sont audités dans le lot catalogue. Le test d’accueil exerce aussi l’interface réelle avec annulation puis réouverture immédiate, un thread hôte et publication après fin. Les attentes explicites de ce thread appartiennent au test, jamais à la boucle UI du firmware.

## Mesures comparables disponibles

Scénario : 120 secondes d’horloge simulée après stabilisation de l’accueil natif, mêmes objets et mêmes fichiers.

| Observation | 0.8.42 | 0.8.43 |
|---|---:|---:|
| Appels d’ouverture de fichiers via l’adaptateur | 5 333 | 0 |
| Appels d’énumération `openNextFile` | 5 302 | 0 |
| Transferts graphiques LVGL pendant cet intervalle | 0 | 0 |
| Objets dans la page native, stables et sans destruction/recréation du contrôle surveillé | 72 | 72 |
| Tables du catalogue allouées via l’allocateur PSRAM simulé, avant première ouverture de bibliothèque | 49 152 octets | 24 576 octets |
| Focus et défilement conservés après ajout d’un ELF à une bibliothèque déjà défilée | Focus seul | Les deux |

La dernière différence d’allocation vient du report du staging jusqu’au premier besoin. **Elle ne constitue pas un gain de SRAM interne, ni une mesure du tas libre de la tablette.** Après ouverture d’une bibliothèque, les deux tables peuvent rester en cache comme prévu. Aucun nouveau tampon de rendu plein écran ni tâche permanente n’est ajouté par le code de page.

## Validation et limites

Les scénarios couvrent l’accueil inactif, l’entrée dans ELF, un ajout externe pendant affichage, une mutation différée sur l’accueil natif, l’annulation au retour ou à l’ouverture d’une modale, la personnalisation et le couple focus/défilement pendant scans identiques ou modifiés. L’interface asynchrone est également exercée en aller-retour rapide pendant annulation. Les sources et tests sont hachés ; leur stabilité pendant le test est vérifiée. Résultats et journaux : `tools/verification/home-ui-0843.json` et `tools/verification/home-ui-0843/`.

Le test ne reproduit pas tous les services de l’OS et n’affirme ni fluidité physique ni disparition des flashs. Les icônes automatiques restent des chargements ponctuels, une par passage du service après un rendu, avec lecture de fichier compatible limitée à 32 Kio. Leur coût sur une SD réelle reste à mesurer lors de l’apparition de l’accueil ; aucune lecture d’icône ne se répète dans le scénario inactif stabilisé. Les symboles personnalisés Calculatrice/Python sont des glyphes de substitution dans ce harness ; les cartes et leur rendu restent réels. La traduction du harness utilise les clés françaises ; il ne remplace pas les contrôles des quatorze packs.

Sur Tab5 : comparer l’accueil inactif puis la souris avec le même profilage et la même SD, relever les phases UI, les accès SD et les compteurs DSI avant/après un flash. Comparer également l’ouverture de la bibliothèque et le retour à l’accueil, sans confondre ce travail ponctuel avec une activité au repos.
