# Tab5OS 0.8.26

<!-- build-status -->
**Compilation complète et contrôles de l’image 0.8.26 réussis.** Preuve : [firmware-verification.json](tools/verification/firmware-verification.json).
<!-- /build-status -->

Cette version reprend les retours sur la 0.8.25. Les manettes, la migration vers `/System`, les médias persistants et les packs de radios restent disponibles.

- **Saisie** : le clic ou le toucher d’un champ reprend la cible du clavier après Tab, y compris dans les dialogues Git et du coffre.
- **Git** : correction d’une double libération de connexion dans un chemin d’échec TLS ; les boutons distinguent Nouveau, Cloner et Configurer. L’identité de l’auteur et les identifiants d’accès au serveur sont séparés.
- **Coffre** : PIN obligatoire au premier accès et lors de la reprise d’un coffre ancien sans PIN, PIN actuel demandé pour le modifier, ouverture différée sur une tâche dédiée. Le bouton s’appelle Copier. Une réinitialisation volontaire reste accessible depuis l’écran verrouillé avec avertissement et saisie de `RESET` ; elle ne récupère pas les secrets ou les clés perdus.
- **RTL-SDR** : onglets plus lisibles, commandes actives selon l’état, champs adaptés et 2 048 détections paginées par 24. Le récepteur conserve le traitement FM corrigé et retire une attente USB inutile après transfert terminé. Il reste natif : un transfert en ELF sans impact mesuré, RDS2 et ADS-B ne sont pas livrés dans cette version.
- **Cartes** : le bouton i ouvre les informations du centre ; l’appui long choisit un point. La recherche filtre les données JSON utiles, réduit les allocations et permet l’annulation.
- **Explorateur et thème** : le deuxième toucher ou double-clic d’une image affiche son aperçu à droite ; toucher cet aperçu ou l’icône image ouvre l’afficheur complet. Le bouton des favoris suit le thème dans ses différents états. La disposition du clavier se choisit séparément, sans interrupteur « lié à la langue ».
- **Série** : USB-C Serial/JTAG ou connecteur RS485, modes Débogage/Données, réglages persistants, commandes `serial` dans le terminal et envoi du texte ou de la sélection depuis l’éditeur.
- **Langues** : les quatorze packs et cinq pages d’aide suivent ces changements.

<!-- resource-summary -->
Les 14 packs couvrent les 3131 clés de référence contrôlées. Factory contient 174 ressources officielles ; le secours firmware en contient 40. Les grandes polices et les dictionnaires de saisie sont distribués sur SD. Preuves : [languages-0826.json](tools/verification/languages-0826.json), [ime-0826.json](tools/verification/ime-0826.json), [resources-0826.json](tools/verification/resources-0826.json).
<!-- /resource-summary -->

Les défauts de focus, de bouton i et de propriété de connexion TLS sont reproduits sur des bancs logiciels avec leurs sources antérieures. Les essais hôte et la compilation ESP32-P4 ne confirment pas à eux seuls la disparition des redémarrages, des flashs bleus ou du bruit radio sur une tablette physique. Aucun flash ni essai physique n’a été réalisé.

Documentation : [Coffre](COFFRE_0826.md), [Radio SDR](RTL_SDR_0826.md), [Cartes](CARTES_0826.md), [Série](SERIAL_0826.md), [Explorateur](EXPLORATEUR.md), [Git](GIT.md), [preuves et limites](VERIFICATION.md).
