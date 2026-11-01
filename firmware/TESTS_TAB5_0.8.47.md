# Tab5OS 0.8.47 — qualification sur appareil

Ces essais n'ont pas été exécutés sur une Tab5 dans l'environnement de développement. Ils déterminent si cette version peut remplacer la 0.8.46 en usage normal. La compilation et les suites hôte sont documentées séparément.

## Préparation et relevés

Installer le BIN applicatif par la mise à jour OTA existante, sans effacer la Flash ni modifier les partitions. Conserver la 0.8.46 et les fichiers personnels. Les réglages, clés et documents doivent rester présents. Utiliser les instructions de `LISEZ_MOI.md` pour les ressources ; ne pas remplacer tout le dossier personnel `System`.

Dans le port de diagnostic configuré, conserver le journal de démarrage. Les commandes sont : `m` pour heaps/capacités/piles, `d` pour affichage et préférences Média, `p` pour activer/désactiver le profilage. Ne pas activer le profilage en permanence pour le test au repos. Un fichier journal ne doit contenir aucun mot de passe ni clé.

Relever `m` et `d` une fois avant les essais, puis après chaque défaut. Noter l'écran affiché, le bouton utilisé, le type de fichier/flux et si le flash touche l'écran entier ou seulement un composant. Pour un reset, garder le panic/backtrace et le journal du redémarrage, pas seulement la dernière ligne.

## Parcours prioritaires

| Essai | Manipulation | Acceptation |
|---|---|---|
| Média | 100 alternances MP3, radio, enregistrements, vidéos ; favoris, recherche, tri et actualisation explicite. Cliquer aussi plusieurs fois sur l'onglet actif. | Aucun écran vide intermédiaire lors d'un remplacement réussi, aucun onglet actif décoché, aucune action sur un autre fichier que celui affiché. Après rechargement suivi d'un manque de mémoire, une liste invalide doit être retirée et l'erreur signalée. |
| Démarrage audio | Lire au moins 10 morceaux MP3/WAV, dont 44,1 et 48 kHz ; arrêt/reprise, pause/reprise, changement de morceau, volume nul puis volume normal. Essayer radio après attente HTTP, puis vidéo après musique. | Aucun claquement ajouté, premières notes présentes, volume conservé ; une seule source audio propriétaire. Noter tout défaut sonore restant. |
| Dessin | Traits courts/longs, gomme, sélection, annuler/rétablir, zoom et défilement ; changer d'application et revenir sur un document non enregistré. | Aucun flash ; pixels et sélection corrects ; document non enregistré conservé. |
| Repos | Dix minutes sur Accueil, puis dix minutes sur Média sans lecture ; garder l'écran allumé et comparer `d` avant/après. | Aucun flash spontané ni reset. Les publications/invalidations doivent correspondre aux composants réellement changeants. |
| Radio + Cartes | Lire une radio, ouvrir Cartes, zoomer avant la fin des chargements, rechercher un lieu ; refaire radio en pause, puis sans Wi-Fi. | Connexion réelle correctement distinguée d'un refus de ressources ; SD disponible ; recherche Internet puis locale, hors ligne local fonctionnel. |
| Éditeur caché | Ouvrir un gros document, commencer son indexation, passer à Média, puis revenir ; répéter sous pression mémoire. Tester avec autosauvegarde désactivée puis activée. | Aucun changement du texte, de l'historique ou de la sélection. Indexation reprend ; les choix d'enregistrement sont respectés. |
| Préférences | Modifier volume, onglet, favoris et sélection, quitter Média, attendre la fin du job, puis redémarrer normalement. Demander aussi la veille immédiatement après une modification. | Réglages retrouvés ; aucune réussite annoncée avant écriture. Une écriture en cours doit être terminée avant la veille, sans attente graphique bloquante. |
| Endurance | 30 minutes de radio avec usages ponctuels de Cartes/Dessin ; 20 cycles ouverture/fermeture des applications. Relever `m` à états identiques. | Aucun reset, SD perdue ou baisse cumulative inexpliquée de mémoire libre/bloc maximal. |

## Lire les diagnostics sans conclure trop vite

- `[DSI] observed=1` rend le compteur `underruns` exploitable. S'il augmente lors d'un flash, le pilote manque réellement de données à cet instant. `observed=0` n'est pas un résultat « zéro erreur ».
- `[DISPLAY transfer]` sépare le nombre de pixels copiés, la durée maximale d'une copie et l'intervalle maximal des callbacks. Un long intervalle peut aussi venir d'une suspension du SDK ; ce n'est pas automatiquement le coût de LVGL.
- `[MEDIA PREFS]` donne les jobs terminés/échoués/différés, leur durée et les underruns observés pendant les écritures. Une coïncidence répétée oriente l'enquête ; elle n'établit pas à elle seule une cause électrique.
- La mémoire minimale est un compteur historique : elle ne remonte pas après fermeture. Comparer la mémoire actuellement libre et le plus grand bloc, à applications/services identiques. DMA recoupe une partie de la mémoire interne ; ne pas les additionner.
- Une sélection ou un document volontairement conservé consomme encore de la mémoire. Une fuite nécessite une diminution cumulative à état identique, pas simplement une différence entre premier démarrage et session utilisée.

## Conditions bloquant une qualification de production

Un flash reproductible, un reset, une perte SD, un fichier altéré, une mauvaise cible de lecture/suppression ou une régression d'enregistrement impose de conserver le journal et de poursuivre la correction. Cette livraison ne porte aucune certification matérielle et ne doit pas être décrite comme « prête pour production » avant ces essais.
