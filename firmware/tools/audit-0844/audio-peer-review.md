# Relecture indépendante des modifications audio 0.8.44

Périmètre : différence `Tab5Audio.cpp/.h` entre la 0.8.43 livrée et les sources 0.8.44, puis inspection de `Tab5AudioPriorityControl`, `Tab5AudioWorkerLifecycle`, de la file de commandes et des consommateurs de snapshots. Relecture statique ; les tests exécutés par l'auteur des modifications restent détaillés dans leurs rapports propres.

Aucun nouveau défaut confirmé de concurrence, de perte de commande ou d'acquittement prématuré des ressources audio n'a été trouvé dans ce périmètre.

- Le passage de la tâche à l'état arrêté publie ses dernières écritures avec release ; le service UI acquiert cet état avant d'utiliser les champs ordinaires. Le seul démarreur reste le service UI. Le nouveau chemin sans tâche vérifie de nouveau le cycle de vie sous le verrou d'état et n'effectue aucune opération de fichier, décodage ou connexion.
- L'acquittement sans tâche ne s'applique qu'au mode Idle sans source, décodeur, fichier d'enregistrement, lease radio, intention de reprise ni commande prioritaire déjà différée. Les générations atomiques empêchent qu'une libération intercalée soit appliquée à un autre propriétaire.
- La file de commandes n'est ni consommée ni réinitialisée dans ce nouveau chemin. Une commande qui arrive pendant le lease Held déclenche toujours le démarrage normal du worker ; ses règles de différé et la priorité de Stop restent celles du gestionnaire existant. Une commande arrivée après la vérification de la file reste en attente pour le prochain service.
- Le worker n'est plus créé uniquement pour un lease Held sans travail. La maintenance Wi-Fi est différée pendant ce lease ; la libération rétablit le fonctionnement normal. Aucune reprise audio fictive n'est créée par l'acquittement Idle.
- La révision média est calculée et publiée sous le même mutex que le snapshot, avec publication release et lecture acquire. Les observateurs conservent chacun leur révision et ne consomment pas celle d'un autre écran. Un verrou occupé ou une copie de chaîne incomplète laisse la révision du consommateur intacte, permettant une nouvelle tentative.
- Les champs surveillés par la révision couvrent les données effectivement affichées par `updateAudioUi` : identité, mode, pause/lecture, volume, statut, progression/durée et enregistrement. Les réglages Wi-Fi conservent leur lecture ordinaire du snapshot ; cette révision n'a pas à devenir un deuxième journal des événements Wi-Fi.

Cette relecture n'établit pas la restitution physique de la pile par les tâches Idle de FreeRTOS, la marge interne réelle ni le comportement des pilotes sur Tab5. Elle ne remplace pas les enchaînements matériels Cartes → Radio et Musique → Vidéo.
