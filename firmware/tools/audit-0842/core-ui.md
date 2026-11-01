# Boucle principale, ergonomie et instrumentation — 0.8.42

## Référence examinée

La référence est l’archive livrée 0.8.41, conservée intacte et identifiée dans `build-baseline.json`. Le firmware antérieur et son ELF donnent les tailles statiques ; ils ne donnent ni le tas libre au démarrage, ni la fragmentation, ni le temps CPU de la tablette. Aucun périphérique physique n’est connecté dans cet environnement.

Le projet conserve son organisation Arduino/C++ au-dessus d’ESP-IDF/FreeRTOS, avec LVGL 9.6, M5Unified/M5GFX et les bibliothèques embarquées existantes. Les modules `Tab5Storage`/`Tab5VolumeFS`, `Tab5ResourceManager`/`Tab5Memory`, `Tab5Task`, `Tab5NetworkGate` et les services applicatifs fournissent déjà les mécanismes transversaux ; aucun ordonnanceur concurrent n’a été ajouté.

## Chemin critique initial

La tâche Arduino UI possède une pile de 24 Kio et une priorité 1. Elle distribue tactile, souris, clavier et manette ; exécute LVGL ; publie les résultats des workers ; entretient les pages et applique les politiques de persistance. Les écritures de l’ancien éditeur, certains changements de catalogue et certaines métadonnées SD restent dans ce contexte. Un `vTaskDelay()` dans ce chemin laisse s’exécuter FreeRTOS mais ne permet pas à cette même tâche de traiter la souris ou LVGL.

Ordre initial : changements de périphériques/page → validation du partage SD → entrées → services mémoire/SD/audio → tick et timers LVGL → services applications → rafraîchissements conditionnels → historique/paramètres/progression/autosauvegarde → sommeil borné. Le sommeil intentionnel est compris entre 1 et 8 ms écran actif, jusqu’à 30 ms écran éteint. Il est conservé.

Constats confirmés avant correction :

- Le tick LVGL était incrémenté après les lectures d’entrée, par un delta calculé avant celles-ci. Les horodatages pouvaient être anciens pendant un callback long. Le callback natif `lv_tick_set_cb(millis)` donne désormais la même horloge monotone aux événements et aux timers ; aucun timer supplémentaire n’est nécessaire.
- La présentation de l’image attendait dans la tâche UI deux événements DSI, jusqu’à 100 ms. Le détail, les tests avant/après et la nouvelle barrière non bloquante sont dans l’audit affichage. La sécurité de propriété du framebuffer est maintenue.
- Le chargement ELF et l’arrêt vidéo se faisaient dans le chemin UI. Ils passent par les tâches déjà présentes et des publications de fin explicites ; la boucle appelle `Tab5Video.service()` et consomme l’erreur de chargement ELF une seule fois. Accepter une demande ne signifie pas que le média ou programme a déjà démarré.
- Les lectures d’état audio créaient des chaînes temporaires superflues, même au repos ; les accesseurs spécifiques lisent désormais leur seul champ sous le verrou existant.

Ce qui n’a pas été modifié faute de défaut démontré : l’actualisation explorateur est déclenchée par une révision, puis regroupée ; le catalogue de l’accueil vérifie la visibilité et travaille par étapes ; le badge Mesh ne se redessine que sur changement de révision ; la libération d’un presse-papiers secret déjà vide retourne immédiatement. Présenter ces mécanismes comme des causes prouvées du lag aurait été inexact.

## Diagnostic de latence

`Tab5UiDiagnostics` est réservé à la tâche UI. Désactivé par défaut et non persistant comme réglage, il se commande dans Réglages → Système, ou par la lettre `p` sur la liaison de diagnostic. Aucun timer, tâche, buffer de document, fichier ni allocation dynamique n’est ajouté. Le nombre de phases et les huit événements sont bornés. Deux journaux RTC occupent ensemble 216 octets ; les tailles statiques finales sont relevées depuis l’ELF.

En mode activé, il mesure le temps écoulé par service, maximum, total, nombre d’appels et nombre d’appels ≥20 ms, plus la durée active et l’intervalle maximal de la boucle. Ce sont des durées incluant attente et préemption, **pas un pourcentage CPU**. Les événements lents sont enregistrés au plus une fois par phase et par seconde ; les compteurs continuent à compter tous les dépassements. Seuls phase et temps sont conservés, jamais chemins, texte saisi ou secrets. Aucune écriture Flash/SD.

La lettre `d` imprime un relevé et les huit événements récents ; `m` imprime les catégories de tas, leurs minima et plus grands blocs, puis l’état stockage. La commande `tasks` du terminal existant donne les piles à la demande. Les détails Informations système montrent le maximum de boucle, l’intervalle et les phases lentes. Après les catégories de reset compatibles avec la conservation RTC, un journal valide du démarrage précédent reste consultable. Coupure électrique, brownout, checksum invalide ou reset pendant une écriture peuvent perdre ce diagnostic : aucune garantie de persistance universelle n’est revendiquée.

L’impression série et l’ouverture des détails sont des actions explicites pouvant elles-mêmes coûter du temps. Pour comparer deux scénarios, relever la mesure puis désactiver/réactiver le profilage pour remettre ses maxima à zéro. Les durées de profilage n’incluent pas le sommeil volontaire dans la durée active ; l’intervalle entre boucles l’inclut.

## Ergonomie préservée

Pas de modification du comportement de navigation, des menus Git, du choix d’autosauvegarde, des noms de documents, ni de la fermeture des applications. Le diagnostic utilise le style des interrupteurs existants et une ligne dans la carte Système adaptative ; aucun écran supplémentaire. Les erreurs différées ELF s’affichent dans le panneau d’exécution existant. Un média en fermeture peut laisser l’interface répondre ; son image n’est libérée qu’après détachement et fin du worker.

## Limites à mesurer et risques résiduels

1. **P1, matériel** : lag généralisé restant, flashs turquoise, contention PSRAM/DMA et sous-alimentation de données DSI. Faire deux minutes d’accueil inactif puis déplacements souris, relever `d`, `m`, les compteurs DSI et la cause de reset. Les tests hôte valident le flot de contrôle et les buffers, pas le bus DSI physique.
2. **P1, panne d’E/S** : la SD peut encore retenir des opérations synchrones sur UI : enregistrement du document de l’ancien éditeur, paramètres/progression, préparation de ressources lors d’une ouverture et commit du partage réseau. Les contrôles d’intégrité sont renforcés, mais cela n’établit pas une borne temporelle du pilote SD. Relever la phase lente et comparer SD présente/absente sans provoquer d’arrachement pendant une écriture.
3. **P2, ouverture** : scan audio/vidéo, préparation initiale de cartes et chargement de source Python ont encore des étapes UI ; mesurer d’abord leur contribution selon la taille réelle des catalogues. Les déplacer exige une publication et une propriété cohérentes, pas seulement un `yield`.
4. **P1, charge native** : un ELF reste du code natif dans l’espace mémoire du firmware. Aucune isolation matérielle n’est ajoutée ou revendiquée ; un programme non coopératif peut encore affecter l’OS.
5. **P2, instrumentation** : aucun chiffre de consommation électrique, de CPU, de mémoire dynamique libre ou de latence réelle sur Tab5 n’a été inventé. Les piles ne sont pas réduites sans high-water marks sur les usages lourds.

Les essais et résultats détaillés sont joints par module. Les constats confirmés, hypothèses matérielles et tests simulés restent séparés dans la synthèse de livraison.
