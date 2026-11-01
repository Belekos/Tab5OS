# Tab5OS 0.8.20 — Wi-Fi et corrections d’interface

<!-- build-status -->
**Compilation complète et contrôles de l’image 0.8.20 réussis.** Preuve : [firmware-verification.json](tools/verification/history-0820/firmware-verification.json).
<!-- /build-status -->

Cette version corrige les régressions signalées après la 0.8.19. Le firmware et le dossier SD fourni doivent être mis à jour ensemble.

## Wi-Fi

La 0.8.19 empêchait les applications d’acquérir le verrou réseau lorsque le réseau était désactivé. Or la routine de connexion désactivait provisoirement les applications avant de prendre ce même verrou : elle ne pouvait donc plus lancer la connexion. Les transitions internes de l’OS utilisent désormais une acquisition distincte, tandis que les accès applicatifs restent bloqués pendant ces transitions.

Le scan, son résultat et les messages d’erreur sont également contrôlés : une recherche différée ou échouée ne doit pas être présentée comme une recherche réussie sans réseau trouvé. Les scénarios d’activation, de désactivation et de nouvelle recherche font l’objet de tests du code de production avec un transport simulé.

Cette correction n’ajoute pas de VPN ni de Tor. Le verrou réseau n’est pas une garantie de routage obligatoire de tous les flux. Voir [RESEAU_DIAGNOSTIC.md](RESEAU_DIAGNOSTIC.md).

## Fichiers et affichage

- L’ouverture de fichiers dans l’éditeur utilise le défilement et un chargement progressif pour dépasser l’ancienne limite de seize entrées.
- Le sélecteur commun de fichiers, notamment celui du lanceur ELF, applique le thème à ses conteneurs, listes et boutons.
- Le catalogue de thèmes présente les thèmes intégrés ainsi que les thèmes enregistrés disponibles.
- Les couleurs des interrupteurs actifs et inactifs sont réglables et conservées avec le thème.
- La calculatrice retrouve une disposition complète : effacement et retour sur le clavier, résultat en bas, commandes de fermeture séparées.
- La largeur réservée à la date dépend du texte et de sa police afin de ne plus couper son premier caractère.

## Ressources et validation

<!-- resource-summary -->
Les 14 packs couvrent les 2714 clés de référence contrôlées. Factory contient 93 ressources officielles ; le secours firmware en contient 38. Les grandes polices et les dictionnaires de saisie sont distribués sur SD. Preuves : [languages-0820.json](tools/verification/languages-0820.json), [ime-0819.json](tools/verification/ime-0819.json), [resources-0820.json](tools/verification/resources-0820.json).
<!-- /resource-summary -->

Les preuves et leurs limites sont regroupées dans [VERIFICATION.md](VERIFICATION.md). Les essais logiciels ne remplacent pas une vérification sur la tablette, en particulier pour la radio Wi-Fi et l’affichage physique. Aucun flash automatique n’est effectué.
