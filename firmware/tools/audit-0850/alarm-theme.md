# Réveil et couleurs de sélection — 0.8.50

## Corrections

- Icône vectorielle de réveil distincte de Média ; index de symbole 16, choix d’icône personnalisé préservé.
- Quatre tonalités : Classique, Carillon, Pulsations et Doux. Synthèse PCM mono 24 kHz, sans allocation supplémentaire, enveloppes de 10 ms aux extrémités des notes. Le pilote et l’arbitrage audio 0.8.49 ne sont pas modifiés.
- Préécoute directement dans le formulaire, à partir de ses valeurs non enregistrées. Le bouton arrête aussi l’essai ; Échap, fermeture et limite de 10 secondes l’arrêtent. La préécoute ne force aucune écriture de réglages. Volume précédent et réservation audio sont restitués par le worker.
- Une vraie échéance survenant pendant la terminaison de la préécoute est conservée, puis lancée après restitution du périphérique.
- Persistance : structure, taille et version NVS existantes conservées ; premier octet réservé utilisé pour la tonalité. Une ancienne valeur inconnue choisit Classique sans perdre les horaires ni réinitialiser les réglages.
- Styles communs : boutons, listes déroulantes et coches suivent la couleur de sélection du thème dans leurs états sélectionnés/pressés. Les filtres natifs LVGL ne recolorent plus ces états. Le style normal des boutons sémantiques et les couleurs distinctes des interrupteurs restent respectés.

## Vérification exécutée

`tools/verification/alarm-theme-0850.json` : vrai LVGL 9.6, modules Réveil, Focus, TextInput et UiStyle de production compilés et exécutés sur hôte avec ASan/UBSan. Les interfaces NVS, RTC, création de tâche et haut-parleur sont contrôlées par le test.

Scénarios : quatre signaux différents, amplitudes bornées et continuité identique malgré les frontières de blocs ; compatibilité des anciens réglages ; préécoute sans sauvegarde et sans remplacement du formulaire ; arrêt, délai de dix secondes, Échap ; échec de création du worker ; restauration du volume et des réservations ; échec d’écriture NVS ; vraie alarme pendant la fin d’une préécoute ; 100 ouvertures/fermetures du formulaire ; couleurs résolues des états checked/pressed/focused/disabled, mise à jour de palette, couleurs personnalisées des interrupteurs. Le compteur brut d’assertions inclut les comparaisons de chaque échantillon PCM : ce ne sont pas autant de scénarios indépendants.

`tools/verification/alarm-theme-target-0850.json` : compilation objet ESP32-P4 de Réveil et UiStyle, avec les en-têtes associés dans les empreintes. La compilation complète appartient au bilan d’intégration.

`tools/verification/alarm-layout-0850.png` : capture hôte 1280 × 720 du vrai formulaire. Les quatre actions et la tonalité restent visibles. Traductions de test anglaises, police Montserrat ; la police TTF OS n’est pas modifiée. Le panneau conserve son défilement pour les polices/traductions plus grandes.

## Limites et essais sur Tab5

Ces tests ne mesurent ni le son analogique, ni les délais matériels, ni le réveil RTC physique. LeakSanitizer est indisponible ici et LVGL hôte emploie CLIB ; aucune mesure de mémoire libre sur Tab5 n’est revendiquée.

Sur Tab5 : écouter chaque tonalité ; démarrer/arrêter une préécoute, la fermer avec Échap et vérifier le volume/reprise de Média ; enregistrer une tonalité, redémarrer et vérifier heure/répétition/volume/tonalité ; programmer une échéance pendant l’arrêt d’une préécoute ; changer de thème et vérifier boutons Média/Dessin/Réveil, coches et listes au tactile et au clavier.
