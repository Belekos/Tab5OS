# Tab5OS 0.8.47 — axes de refonte et limites de production

## Point de départ

Le retour sur la 0.8.46 constitue un échec de validation matérielle des symptômes : flashs communs aux applications, Média particulièrement touché, claquement au démarrage musical. Les résultats hôte antérieurs restent des contrôles de code, pas la preuve que ces symptômes avaient disparu.

La référence est le ZIP 0.8.46 livré, SHA-256 `22d99628cc3ab6a9673be1886755ec9a3092aaede3f017a7e0e20443cc0816a6`. Il reste récupérable. La refonte porte sur des mécanismes communs existants ; elle conserve LVGL, FreeRTOS, Arduino/ESP-IDF, M5Unified/M5GFX et les applications.

## 1. Un seul propriétaire de l'affichage

Dans les chemins modifiés, la tâche UI conserve la propriété des objets LVGL et de la navigation. Le présentateur conserve deux images DSI et ne modifie que l'image retirée du balayage. Les événements natifs du pilote restent l'autorité pour leur réutilisation. Le nouveau worker de préférences ne manipule aucun objet LVGL ; cette organisation ne constitue pas une isolation du code ELF externe.

Le transfert LVGL RGB565 vers l'image physique est spécialisé pour la géométrie Tab5 vérifiée : transposition par blocs, ordre des octets explicite, validation des dimensions, rotations et capacités. Le chemin M5GFX demeure disponible si les préconditions du transfert spécialisé ne sont pas réunies. Cela remplace un chemin générique coûteux, sans nouvelle image plein écran ni modification arbitraire des horloges de dalle.

Les compteurs distinguent invalidations LVGL, publications d'images et underruns DSI. Une baisse du nombre de pixels retravaillés n'est pas assimilée à une mesure de fluidité ou à une disparition du flash physique.

## 2. Une vue Média conservée jusqu'à son remplacement complet

La liste prépare un modèle de références en PSRAM et le publie une fois prêt. Une allocation impossible conserve le modèle visible et sa sélection si sa révision source est toujours valide. Si le catalogue a été rechargé, ses anciennes références sont retirées pour empêcher une lecture ou une suppression visant un autre fichier, et l'échec reste signalé. Les lignes, libellés et styles conservés ne sont modifiés que lorsque leur valeur change. Un onglet exclusif reçoit son état depuis la navigation validée ; il ne se décoche pas indépendamment avant le callback.

Le tri utilise une arène de noms au lieu d'une allocation par élément. Les données et objets ont une propriété explicite ; le deuxième tableau de références augmente la mémoire résidente de la vue jusqu'à sa récupération ou sa destruction, coût mesuré et assumé pour éviter la disparition intermédiaire du contenu. Les accès aux métadonnées SD restent bornés par le catalogue et n'entrent pas dans le comparateur. Leur premier chargement demeure synchrone : ce point n'est pas présenté comme une indexation entièrement asynchrone.

## 3. Des préférences durables sans attente NVS dans les actions Média

La tâche UI conserve l'état courant. Après le délai de regroupement existant, elle confie une copie immuable à une tâche de persistance temporaire de priorité 1, sans affinité forcée. Un seul job existe à la fois. Son instantané est en PSRAM ; sa pile reste interne, car le SDK intervient avec les caches désactivés pendant certaines opérations Flash.

Le worker publie sa fin avec une relation acquire/release. L'UI ne libère l'instantané qu'après cette publication, et ne considère pas une ancienne copie comme l'enregistrement d'une modification plus récente. Les formats et clés NVS existants sont conservés. Les erreurs laissent les changements à enregistrer ; les reprises gardent leur limite de fréquence. Une demande de veille est différée tant que ces préférences ne sont pas enregistrées, sans annoncer prématurément une réussite.

Ce mécanisme ne sauvegarde aucun document et ne change aucune politique d'autosauvegarde. Il ne supprime pas les pauses globales du SDK Flash : déplacer NVS sur un autre cœur ne peut pas annuler une suspension des deux cœurs.

## 4. Un démarrage audio ordonné par le matériel

La sortie prépare I²S et les horloges avant d'activer le codec et l'amplificateur. Un transfert DMA de silence valide la préparation ; le codec reste muet pendant sa configuration. Les erreurs matérielles remontent et les ressources sont nettoyées. La séquence n'ajoute pas de suppression des premiers échantillons, de délai arbitraire ou de fondu destiné à masquer un défaut. La présence des premières notes et l'absence de claquement restent à vérifier acoustiquement.

La correction M5Unified est isolée, opt-in pour Tab5OS et reproductible depuis la dépendance verrouillée. La compilation doit vérifier qu'elle utilise bien la copie corrigée ; une modification locale oubliée dans une bibliothèque personnelle n'est pas une méthode de livraison.

## 5. Des ressources communes attendues avec FreeRTOS

Le verrou d'admission mémoire devient un mutex natif à héritage de priorité. Les workers attendant une collecte dorment sur un événement de fin, sans réveil à chaque tick. Les budgets, marges, capacités et protections des données visibles restent conservés.

Les demandes de libération sont exécutées par le propriétaire des caches. Une expiration pendant un callback ne permet jamais de réutiliser sa mémoire avant son retour. Une attente résiduelle de libération différée de pile par les tâches idle ne doit pas être supprimée en libérant de force une tâche.

Les deux cœurs sont déjà employés : UI/Cartes/réception réseau sur le cœur 1 ; décodage, haut-parleur, vidéo et plusieurs services sur le cœur 0. La refonte ne déplace pas aveuglément les traitements vers le cœur audio. Les calculs de présentation d'un éditeur caché peuvent être suspendus ; ses documents et ses enregistrements autorisés doivent continuer à respecter leur contrat.

## Ce qui sépare cette version d'une qualification de production

La compilation, les scénarios d'erreurs et les tests de concurrence sont nécessaires mais insuffisants. Il reste à mesurer sur Tab5 : absence de flash à l'action et au repos, signal audio initial, marge réelle des piles, mémoire minimale et bloc contigu, coexistence radio/Cartes, stabilité des périphériques et redémarrages.

Le SDK fourni utilise la PSRAM 200 MHz et un cache L2 de 256 Kio ; il ne fournit pas une variante XIP immédiatement interchangeable. Activer XIP nécessite reconstruire le SDK et son démarrage, consomme plusieurs Mio de PSRAM et peut accroître le trafic de ce bus. Définir une macro dans le sketch ne corrige pas les bibliothèques précompilées. Cette piste reste une évolution distincte à qualifier, pas une fausse correction appliquée à cette livraison.

Les ELF natifs partagent la mémoire du firmware : aucun isolement matériel entre processus n'est revendiqué. Aucun changement de table de partitions, de format de clés ou de données SD, ni de politique d'autosauvegarde n'est introduit. La conservation après mise à jour et les critères d'acceptation restent à vérifier selon `TESTS_TAB5_0.8.47.md`.
