# Tab5OS 0.8.31 — recherches successives et mémoire

Cette reprise suit le retour de la 0.8.30 : une première recherche Cartes aboutit, la suivante plante ; Git refuse de démarrer avec 46 Kio de RAM interne libres pour 48 Kio requis. La PSRAM disponible est abondante, mais ne remplace pas la RAM interne requise par certaines piles de tâches et certains pilotes.

<!-- build-status -->
**Compilation complète 0.8.31 et contrôles du firmware réussis sous Linux/GCC.** Preuve : [firmware-verification-0831.json](tools/verification/firmware-verification-0831.json). Aucun essai sur Tab5 physique ni téléversement n’a été effectué.
<!-- /build-status -->

## Cartes : détacher la saisie quand la fenêtre disparaît

Un défaut supplémentaire a été reproduit avec LVGL réel : le service de saisie conservait l’adresse du champ de recherche après la destruction de sa fenêtre. Il pouvait ensuite la consulter pendant son entretien périodique ou à l’ouverture d’une nouvelle saisie. Ce parcours existe aussi avec un clavier latin ; il ne dépend pas des mots séparés par des espaces.

Le service suit maintenant la suppression du champ par son événement de destruction. Il détache l’ancien champ, vide la composition et ses candidats, et suit le nouveau champ seulement si son écouteur de destruction a pu être installé. Il ne tente plus de valider une adresse déjà libérée. Les protections clavier de la 0.8.30 sont conservées.

Le banc reproduit l’accès libéré de la 0.8.30 sous AddressSanitizer. Le correctif passe 400 cycles de fenêtres de recherche latines et 300 cycles de changement/suppression de saisie CJK, avec LVGL, TinyTTF et les classes de saisie réels. Cela établit un défaut du logiciel ; aucun plantage précis n’a été reproduit sur une tablette physique ni pendant une requête réseau réelle.

## Git : prévoir le besoin complet avant de lancer la tâche

Le message transmis situe l’échec après la création de la tâche Git et de sa pile interne de 40 Kio. Il restait 46 Kio pour une demande exigeant 48 Kio, réserve système comprise. Libérer uniquement des tuiles en PSRAM ne pouvait pas résoudre ce manque de RAM interne.

Le budget de session et TLS est désormais réservé avant la création de la tâche. L’admission de la pile voit cette réservation en cours et demande à l’OS une récupération couvrant le besoin combiné. La tâche reprend ensuite la réservation, sans répéter l’admission après avoir alloué sa pile. Les échecs rendent les réservations, et le budget de session est conservé jusqu’à la fin pour les allocations SDK non suivies individuellement.

La pile de 40 Kio, la marge interne de 32 Kio, la réserve PSRAM de 512 Kio et le plafond Git suivi de 8 Mio restent inchangés. La réservation progressive introduite en 0.8.30 est conservée. Si les ressources réellement disponibles restent insuffisantes, le refus demeure explicite : cette modification ne garantit pas qu’un dépôt arbitraire tiendra en mémoire.

Après un échec de **Cloner** ou **Nouveau dépôt**, l’interface ne lance plus un `git status` supplémentaire qui ajoutait « Aucun dépôt Git compatible dans le dossier » et masquait le diagnostic initial.

## L’OS récupère les ressources selon leur usage réel

Le gestionnaire central existant distingue maintenant ce qu’un cache peut libérer : RAM interne, PSRAM ou les deux. Une pénurie de RAM interne ne déclenche plus la vidange d’un cache déclaré uniquement en PSRAM. Les décisions tiennent compte des réservations concurrentes et des plus grands blocs libres, pas seulement du total disponible.

Les ressources rechargeables déclarées par les applications expirent aussi après 30 secondes en arrière-plan, avec au plus un rappel d’expiration par seconde. La récupération reste exécutée par le service de l’OS ; une tâche de fond peut la demander avec une attente bornée. L’application au premier plan et les ressources encore utilisées sont protégées par les contrôles du gestionnaire et de chaque application. Les documents modifiés, lectures et enregistrements actifs ne sont pas supprimés pour obtenir de la mémoire.

La tâche audio n’est plus conservée sans travail : elle démarre à la demande, sous admission de l’OS, puis se termine elle-même lorsqu’elle n’a plus de décodeur, d’enregistrement, d’opération Wi-Fi ou de commande à traiter. Sa pile interne de 16 Kio est rendue par le nettoyage normal de FreeRTOS. La file de commandes conserve ses données ordinaires en PSRAM et son contrôle FreeRTOS en RAM interne, comme dans la 0.8.30. La récupération ne détruit pas une tâche audio active. Les opérations ou sessions prioritaires qui ont encore besoin de la tâche la maintiennent en vie. Un entretien Wi-Fi facultatif peut recréer brièvement la tâche toutes les cinq secondes ; il ne contourne pas l’admission mémoire et aucun décodage ou appel réseau lourd ne retombe sur la tâche d’interface. Les tests vérifient l’ordre des libérations avec un tas simulé ; le gain de RAM sur la tablette n’a pas été mesuré ici.

## Fonctions conservées et vérifications

Aucune interdiction Wi-Fi fondée sur le numéro de version C6 n’est réintroduite. Une alerte batterie ne force pas la sauvegarde de l’éditeur ; l’autosauvegarde ordinaire reste optionnelle. La surveillance batterie, les diagnostics et les protections clavier précédentes sont conservés.

Les résultats et limites figurent dans [VERIFICATION.md](VERIFICATION.md). Les tests hôtes, la compilation cible et les essais physiques sont trois niveaux distincts. La disparition des incidents sur votre tablette reste à confirmer avec le firmware livré.
