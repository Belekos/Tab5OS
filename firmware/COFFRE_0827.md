# Coffre — correction ciblée 0.8.27

Le signalement matériel est précis : le message « Coffre déverrouillé » apparaît, puis la tablette redémarre. Aucun essai physique n'a été effectué pour cette livraison ; la disparition de ce redémarrage n'est donc pas confirmée.

## Défaut établi et comportement corrigé

Dans la 0.8.26, le worker publiait son succès dès l'ouverture du trousseau, avant le chargement des deux banques d'identifiants sur la SD. Le retour d'erreur de ce chargement était ignoré. Si les banques étaient altérées ou la lecture échouait, la reconstruction de la page appelait à nouveau la lecture et le déchiffrement depuis la tâche de l'interface. Les appels suivants recommençaient également ces opérations.

La 0.8.27 affiche « Lecture des identifiants… » pendant ce travail. L'échec reste mémorisé pour la session, sans reprise implicite de lecture depuis l'interface. L'écran du gestionnaire affiche l'erreur et propose **Réessayer**, qui relance un worker. Verrouiller puis déverrouiller permet également une nouvelle tentative.

Les clés de chiffrement restent accessibles lorsque le PIN est correct mais que seuls les identifiants SD sont indisponibles. Une SD absente ou deux banques vides/altérées ne sont jamais assimilées à un coffre neuf : aucune banque existante n'est remplacée. La récupération volontaire par **Coffre inaccessible ?** et confirmation `RESET` reste disponible, avec les conséquences décrites dans `COFFRE_0826.md`.

La lecture des banques rend la main entre les blocs déchiffrés et vérifie que la session n'a pas été verrouillée. Le format chiffré, les clés, les PIN existants et la dérivation des sauvegardes portables restent compatibles.

## Si le redémarrage persiste

Un marqueur RTC dédié conserve la dernière phase observée du parcours PIN appareil : `unlock`, `auth`, `preload`, `await-ui`, `ui-rebuild`, puis `ready` une fois l'interface prête. La fermeture utilisateur termine correctement la continuation, y compris si le worker est encore actif. Le chemin API sans interface termine au retour du worker. Les marqueurs n'enregistrent aucun PIN, identifiant, secret ou adresse et n'écrivent pas sur la SD.

Après un redémarrage compatible avec la conservation RTC, le diagnostic de démarrage peut indiquer cette phase. C'est un repère pour localiser le problème, pas une preuve de sa cause. Il complète le message d'exception et la pile du journal série. Le worker historique à mot de passe n'a pas ce suivi PIN détaillé ; sa reconstruction UI reste marquée. Voir `DIAGNOSTICS_COFFRE_0827.md` pour les limites de conservation RTC.
