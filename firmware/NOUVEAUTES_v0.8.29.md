# Tab5OS 0.8.29 — Wi-Fi et sauvegarde de l’éditeur

Cette reprise corrige deux comportements de la 0.8.28.

<!-- build-status -->
**Compilation complète 0.8.29 et contrôles du firmware réussis sous Linux/GCC.** Preuve : [firmware-verification-0829.json](tools/verification/firmware-verification-0829.json). Aucun essai sur Tab5 physique ni téléversement n’a été effectué.
<!-- /build-status -->

## Wi-Fi

La version du firmware C6 ne sert plus à refuser une recherche de réseaux ou une connexion. La requête RPC de version ajoutée avant ces opérations est supprimée : une absence de réponse à cette requête ne peut donc plus en retarder le lancement. Les commandes passent de nouveau par le chemin Wi-Fi normal du SDK.

Une version inconnue ou différente n’est plus une interdiction décidée par Tab5OS. Une incompatibilité réelle du protocole, du transport ou de la configuration peut toutefois empêcher le pilote de fonctionner. Aucun nouveau protocole C6 et aucun flash automatique ne sont ajoutés. Voir [COMPATIBILITE_TAB5_0.8.29.md](COMPATIBILITE_TAB5_0.8.29.md).

## Éditeur et batterie

La batterie critique ne déclenche plus d’enregistrement du document ni de copie de secours. Le traitement automatique ajouté en 0.8.28 est retiré entièrement. L’option d’autosauvegarde ordinaire garde son fonctionnement et son réglage ; elle n’est pas activée par cette correction.

Les mesures de batterie, avertissements et décompte d’arrêt en décharge critique restent présents. Si l’autosauvegarde est désactivée, les modifications non enregistrées restent en mémoire et peuvent être perdues à l’arrêt, conformément au choix de ne pas les enregistrer automatiquement.

## Compilation et limites

La reconstruction Linux/GCC a nécessité des corrections de typage dans les calculs de dimensions LVGL de Git et du SDR, les compteurs de répétition PCRE2 et des pointeurs/callbacks internes à libgit2. Elles alignent les types sur les API existantes, sans ajouter de fonctions à Git ou au SDR. Les patches, leur provenance et les contrôles de syntaxe cible sont détaillés dans [COMPILATION.md](COMPILATION.md). La [procédure Linux/GCC](tools/build_linux/README_LINUX.md) accompagne les sources ; les alternatives Windows ne sont pas réexécutées pour cette version.

La détection écran/tactile, les informations sur la révision P4, les commandes série de l’éditeur et les autres fonctions de la 0.8.28 sont conservées. Les notes 0.8.28 restent historiques ; leurs descriptions du refus Wi-Fi selon la majeure C6 et de la copie de secours avant coupure ne s’appliquent plus.

Les résultats effectifs figurent dans [VERIFICATION.md](VERIFICATION.md). Aucun essai physique ni téléversement n’est revendiqué.
