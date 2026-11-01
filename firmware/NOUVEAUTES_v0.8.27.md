# Tab5OS 0.8.27 — coffre et série dans l’éditeur

Cette mise à jour reprend les deux problèmes signalés après la 0.8.26 : le redémarrage juste après le message de déverrouillage du coffre et l’absence de commandes série visibles dans l’éditeur réellement utilisé.

<!-- build-status -->
**Compilation complète et contrôles de l’image 0.8.27 réussis.** Preuve : [firmware-verification.json](tools/verification/firmware-verification.json).
<!-- /build-status -->

## Coffre

Le chargement des identifiants dispose désormais d’un état distinct. Une lecture ou un déchiffrement en échec ne déclenche plus une nouvelle tentative depuis l’interface à chaque affichage. La page conserve et montre l’erreur de la session ; le travail de chargement reste dans le worker. Après remise en place de la carte SD, **Réessayer** relance explicitement ce chargement.

La 0.8.26 pouvait annoncer le déverrouillage avant la fin du chargement des identifiants. Ce chemin logiciel est vérifié séparément du matériel. Aucun redémarrage physique du Tab5 n’a été reproduit pendant le développement : les essais sur ordinateur ne suffisent pas à établir toutes les causes possibles du reset signalé.

## Éditeur et série

La commande précédente avait été placée dans l’ancien éditeur de compatibilité. Le véritable éditeur dispose maintenant d’un bouton **Série** dans sa barre, qui ouvre un volet utilisable à côté du document.

- **Envoyer** prend la sélection ou, sans sélection, le document entier, y compris ses passages non affichés.
- **Écouter** reçoit le texte dans un volet séparé ; **Insérer** le place dans le document. La réception ne modifie pas le fichier automatiquement.
- **Arrêter**, **Annuler l’envoi** et **Effacer la réception** agissent séparément.
- Réduire le volet conserve l’écoute ; quitter l’éditeur arrête sa session.
- L’éditeur et le terminal ne peuvent pas consommer simultanément les mêmes octets reçus.

Le port et ses paramètres restent dans Réglages. Le volet propose le mode Données pour les échanges. Les commandes du terminal permettent aussi d’envoyer des fichiers et de rediriger des flux binaires. Voir [SERIAL_0827.md](SERIAL_0827.md).

## Traductions et vérifications

Les libellés et l’aide sont actualisés dans les quatorze langues. Les preuves de la 0.8.26 sont conservées avec leurs dates et leurs empreintes ; elles ne sont réutilisées que si les sources et les bancs de test concernés restent identiques. Les parcours modifiés sont rejoués. [VERIFICATION.md](VERIFICATION.md) présente les résultats réellement validés pour la livraison.

<!-- resource-summary -->
Les 14 packs couvrent les 3148 clés de référence contrôlées. Factory contient 174 ressources officielles ; le secours firmware en contient 40. Les grandes polices et les dictionnaires de saisie sont distribués sur SD. Preuves : [languages-0827.json](tools/verification/languages-0827.json), [ime-0827.json](tools/verification/ime-0827.json), [resources-0827.json](tools/verification/resources-0827.json).
<!-- /resource-summary -->

Le firmware, les données SD et les fichiers de diagnostic correspondent à cette version. Aucun téléversement ni essai physique USB-C/RS485 n’est revendiqué.
