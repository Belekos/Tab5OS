# Tab5OS 0.8.14

> Note historique. La version courante est la 0.8.16 : Mail, Internet, Quake et Duke Nukem 3D sont retirés de la distribution principale. Lire NOUVEAUTES_0.8.16.md et VERIFICATION.md pour son périmètre et ses validations.

- Internet : véritable mise en page HTML/CSS litehtml, images JPEG/PNG, tableaux, liens dans le texte, formulaires GET simples et commandes de saisie de l’OS. Cache borné et libéré à la fermeture. Pas de JavaScript, cookies ou formulaires POST.
- Mail : bouton de configuration Gmail, indications du mot de passe d’application Google, affichage HTML avec le même moteur et rédaction dans la zone d’écriture de l’OS. Les images distantes des courriels restent bloquées. OAuth n’est pas intégré.
- Coffre : suppression du délai de cinq minutes. Verrouillage sur demande ou à l’extinction de l’écran.
- Applications : GBA, SNES, MegaDrive, Doom, Quake et Duke3D, avec sélection des fichiers depuis le lanceur ou Ouvrir avec. Les programmes utilisent les entrées, volume et affichage de l’OS. Les jeux commerciaux ne sont pas fournis.
- SDK ABI 4 : budgets PSRAM explicites, piles étendues contrôlées, commandes supplémentaires de manette, sauvegardes bornées et services numériques partagés.
- Traductions : nouveaux messages et aide Mail/Internet/coffre dans les huit packs fournis.

Le socle LVGL/M5Stack et les fichiers du pipeline d’affichage stable ne changent pas. Installer le firmware **et** actualiser les ressources SD. Les six nouveaux programmes figurent dans Applications après copie des ELF ou réparation des ressources officielles via Factory.

Les tests sur ordinateur ne remplacent pas les essais physiques. Voir VERIFICATION.md pour les contrôles effectués et les points restant à vérifier, notamment la cadence des jeux et votre compte Gmail.
