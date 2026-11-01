# Réglages — 0.8.25

Les cartes s'ajustent maintenant à leur contenu. Les lignes réservent au moins 48 pixels aux actions tactiles ; les petits curseurs et interrupteurs ont une zone tactile étendue. Les libellés peuvent passer à la ligne au lieu d'être tronqués. Les commandes suivent la largeur disponible quand le bandeau latéral est affiché ou masqué.

- **Interface** : commandes d'affichage et d'édition regroupées ; souris sur deux lignes ; informations de manette dans une carte qui grandit seulement lorsqu'un rapport est affiché.
- **Système** : horloge et Wi-Fi regroupés ; moins d'espace vide dans Énergie et Partage. Les informations de partage occupent de la place seulement quand elles sont présentes.
- **Langue** : langue, clavier et dossier Documents conservent leurs propres lignes ; les descriptions longues restent visibles.
- **Coffre de mots de passe** : une entrée directe dans Réglages, distincte des clés de chiffrement.

Le bouton mobile des interrupteurs possède un contour de deux pixels, visible à l'arrêt et à l'activation. Son contour reste visible si un thème personnalisé lui donne la même couleur que le fond de l'interrupteur. Les quatre couleurs configurables des interrupteurs et le réglage d'arrondi sont conservés. L'aperçu de thème utilise la même règle.

## Vérification

`tools/settings-tests/run_layout.py` exécute la création actuelle de Réglages avec LVGL 9.6, la police distribuée à 24 pixels et les 14 packs linguistiques. Les appels métier et callbacks vers le matériel sont simulés ; le test concerne la construction, les styles et la géométrie. Il vérifie les limites de chaque ligne et carte, les cibles tactiles, les rapports de manette longs, les informations de partage et les transitions 1216 → 1280 → 1216 pixels.

`tools/ui-tests/run.py` vérifie aussi les contours des interrupteurs sur des couleurs noires, blanches, grises et bleues, dans les deux états et pour trois arrondis, ainsi que l'aperçu de thème réel. Ces contrôles sont exécutés sur ordinateur et ne constituent pas un essai physique de la tablette.
