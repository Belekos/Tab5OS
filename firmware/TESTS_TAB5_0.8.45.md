# Tests physiques nécessaires — Tab5OS 0.8.45

Aucun de ces essais n'a été exécuté sur tablette pendant la correction. La compilation et les tests hôte sont dans `VERIFICATION_0.8.45.md`. Préserver la carte et ses fichiers ; ne pas formater, effacer NVS, les clés ou réinitialiser les préférences. Utiliser la procédure OTA habituelle ; ce BIN n'est pas une image fusionnée à écrire à l'adresse zéro.

## Capture minimale

Ouvrir la liaison série à 115200 bauds et conserver le début du démarrage : version, cause du dernier reset et `double buffering: active/unavailable`.

- `m` avant/après un incident : mémoire libre/minimale, plus grand bloc interne, PSRAM, puis SD `utilisateurs`, `fichiers`, `exclusif`, `disponible`, erreur et essais.
- `d` avant/après un flash : configuration demandée de l'écran, soumissions, transferts LVGL et underruns DSI. Cela n'ouvre pas une autre page.
- `p` seulement si une lenteur doit être profilée : activer, reproduire brièvement, `d`, puis `p` pour arrêter le profilage.

Ne pas envoyer ces commandes en boucle. Masquer toute URL contenant un token, tout identifiant ou secret dans les journaux transmis.

## SD et zoom libre

1. Démarrer, relever `m`, ouvrir Cartes et zoomer/dézoomer **avant que les tuiles précédentes aient fini de charger**, une cinquantaine de fois. Ajouter des déplacements. Cette liberté doit rester disponible.
2. Relever `m`, puis essayer un petit nouveau fichier volontaire dans l'Explorateur, sans écraser un document existant. Si la SD devient indisponible, demander sa reconnexion dans Réglages et conserver tous les logs jusqu'au résultat.
3. Vérifier les compteurs : des fichiers réellement actifs peuvent empêcher le démontage ; fermer normalement leurs applications puis réessayer. Une ouverture ratée ne doit plus laisser un fichier fantôme empêchant la reconnexion.
4. Répéter cinq cycles Cartes → Média → radio, sans redémarrage entre les cycles. Comparer plus grand bloc interne et références SD.

Un `ENOMEM` ou `ENOSPC` ne doit plus lancer un nouveau remontage de bus. Une EIO/absence physique reste à investiguer avec sa cause exacte. Ne pas remplir artificiellement une carte contenant des données pour tester.

## Recherche des lieux

- Connecté : rechercher une ville connue. Internet doit être essayé en premier.
- Sans connexion : rechercher la même ville et un lieu/Mesh enregistré ; le local doit fonctionner selon l'index installé.
- Wi-Fi connecté mais Internet indisponible, ou service renvoyant une erreur/aucun résultat : le repli local doit rester possible. Le journal technique conserve le résultat de l'essai en ligne.
- Pendant une radio : le local doit rester accessible sans attendre indéfiniment le bail réseau.
- Changer de zoom pendant une recherche puis lancer une autre recherche : seule la dernière recherche doit présenter ses résultats. Le zoom ne doit pas annuler une recherche valide simplement parce que sa génération de tuiles change.

L'index GeoNames local ne couvre pas toutes les rues/POI ; les tuiles hors ligne et l'index de lieux sont des ressources distinctes. Noter le nom d'une recherche sans résultat, sans données sensibles.

## Radio et claquement MP3

1. MP3 local → radio → autre station → vidéo → retour Média → enregistrement, dix cycles. Tester l'arrêt pendant une ouverture réseau et une coupure volontaire Wi-Fi.
2. Tester un MP3 44,1 kHz, puis une autre fréquence connue. Distinguer le claquement initial d'un décrochage ou d'un flash d'écran.
3. Relever les lignes I2S : la première fréquence MP3 ne doit plus provoquer un deuxième arrêt/redémarrage de la sortie. Conserver erreurs HTTP/TLS et le démarrage suivant si un reset survient.
4. Respecter les choix d'enregistrement ; enregistrer manuellement les fichiers de test souhaités. La vidéo ne doit pas interrompre un enregistrement pour récupérer sa sortie.

Les tests hôte prouvent la correction de durée de vie et le retrait des redémarrages superflus ; ils ne remplacent pas l'écoute et le streaming réels.

## Flashs Dessin/Média : priorité de diagnostic restante

1. Envoyer `d`, ouvrir Dessin et faire dix petits traits/clics. Envoyer `d` après un flash, noter s'il touche toute la dalle ou seulement l'image.
2. Même comparaison pendant les changements de volets et boutons Média, puis deux minutes d'accueil au repos.
3. Conserver `active/observed`, type de configuration demandée, soumissions, `frames/largeFrames` et `underruns` avant/après, ainsi que la révision de dalle si connue.

Le scan DSI continue au repos : une hausse de `scans` seule n'est pas un rendu demandé. Les **underruns ne devraient pas augmenter** en fonctionnement sain. Si `observed=0`, zéro underrun ne constitue pas une mesure. Une hausse au moment du flash oriente l'enquête sur le transport/bande passante ; son absence ne prouve pas à elle seule une cause matérielle ou LVGL.

Pour poursuivre ce point, transmettre les deux relevés `d` entourant un flash et le journal de démarrage. Sans ces données physiques, il n'est pas justifié de modifier arbitrairement les timings de la dalle.
