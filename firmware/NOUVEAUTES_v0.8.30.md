# Tab5OS 0.8.30 — clonage Git et recherche de lieux

Cette reprise porte sur deux défauts signalés après la 0.8.29.

<!-- build-status -->
**Compilation complète 0.8.30 et contrôles du firmware réussis sous Linux/GCC.** Preuve : [firmware-verification-0830.json](tools/verification/firmware-verification-0830.json). Aucun essai sur Tab5 physique ni téléversement n’a été effectué.
<!-- /build-status -->

## Git : admission mémoire progressive

Le lancement Git exigeait une réservation préalable de 8 Mio de PSRAM, avant même de connaître les allocations nécessaires. Les tests reproduisent ainsi un refus avec 4 Mio libres alors que le petit ensemble de données utilisé par l’essai tient en mémoire. Ce défaut avéré explique un refus trop strict ; il ne prouve pas à lui seul la cause de tous les échecs constatés sur tablette.

La réservation devient progressive, par tranches de 256 Kio, avec une demande de taille exacte après récupération mémoire lorsqu’une tranche ne tient pas. Le plafond de 8 Mio reste appliqué aux allocations suivies, en comptant les en-têtes et les blocs temporaires d’une réallocation. Les réserves du système, la mémoire TLS et la pile interne de 40 Kio restent nécessaires. Le lancement conserve une réserve pour les tampons et TLS ; il ne demande plus d’emblée le plafond complet.

Les messages distinguent les limites de SRAM, de PSRAM, du plus grand bloc disponible et l’échec de création de la tâche après admission. Les essais de l’allocateur et du gestionnaire de ressources passent avec 400 sessions répétées. Ils simulent le tas et sa fragmentation : aucun clone réseau réel ni clone sur Tab5 n’a encore été effectué ici.

## Cartes : validation clavier et durée de vie des fenêtres

Un accès à un objet déjà libéré a été reproduit avec LVGL 9.6 et TinyTTF réels : valider un résultat de recherche par Entrée ferme la fenêtre et détruit la liste, puis la navigation clavier réutilisait son ancien parent. L’activation par Espace emprunte le même parcours. Un autre accès devenu invalide existait après la validation d’un champ monoligne dont le traitement détruit la fenêtre. La prise de focus initiale pouvait aussi relire un contrôle détruit par son propre événement de focus.

La navigation suit désormais la destruction du parent avant de rétablir le focus. L’état d’édition d’un champ monoligne est ajusté avant de déclencher sa validation, ce qui évite de relire le champ détruit ou d’écraser l’état d’une nouvelle fenêtre ouverte par le traitement. La prise de focus initiale respecte également la destruction du contrôle pendant cet événement. Ces corrections portent sur la gestion du clavier partagée par les applications.

Le nombre de mots n’est pas établi comme cause : les espaces étaient déjà encodés dans la requête de recherche. Le défaut logiciel reproduit concerne une validation de l’interface ; sans connaître l’étape exacte du plantage sur tablette, il reste à confirmer qu’il correspond au cas signalé. Les tests reproduisent les quatre parcours fautifs de la 0.8.29 sous AddressSanitizer, puis les parcourent sans cette erreur avec le correctif. Ils vérifient aussi le parseur et l’encodage existants avec ArduinoJson réel, y compris des échecs d’allocation. Le bilan est détaillé dans [maps-0830.json](tools/verification/maps-0830.json).

## Fonctions conservées

Les corrections 0.8.29 restent présentes : aucune interdiction Wi-Fi fondée sur le numéro de version C6, aucune sauvegarde de l’éditeur forcée par une alerte batterie. L’autosauvegarde ordinaire reste optionnelle, selon le réglage existant. La surveillance batterie et les diagnostics écran sont conservés.

Les résultats effectifs sont consignés dans [VERIFICATION.md](VERIFICATION.md). Aucun essai physique ni résolution confirmée sur tablette n’est revendiqué.
