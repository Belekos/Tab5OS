# Coffre — correction ciblée 0.8.27

Le signalement matériel est précis : le message « Coffre déverrouillé » apparaît, puis la tablette redémarre. Aucun essai physique n'a été effectué pour cette livraison ; la disparition de ce redémarrage n'est donc pas confirmée.

## Défaut établi et comportement corrigé

Dans la 0.8.26, le worker publiait son succès dès l'ouverture du trousseau, avant le chargement des deux banques d'identifiants sur la SD. Le retour d'erreur de ce chargement était ignoré. Si les banques étaient altérées ou la lecture échouait, la reconstruction de la page appelait à nouveau la lecture et le déchiffrement depuis la tâche de l'interface. Les appels suivants recommençaient également ces opérations.

La 0.8.27 affiche « Lecture des identifiants… » pendant ce travail. L'échec reste mémorisé pour la session, sans reprise implicite de lecture depuis l'interface. L'écran du gestionnaire affiche l'erreur et propose **Réessayer**, qui relance un worker. Verrouiller puis déverrouiller permet également une nouvelle tentative.

Les clés de chiffrement restent accessibles lorsque le PIN est correct mais que seuls les identifiants SD sont indisponibles. Une SD absente ou deux banques vides/altérées ne sont jamais assimilées à un coffre neuf : aucune banque existante n'est remplacée. La récupération volontaire par **Coffre inaccessible ?** et confirmation `RESET` reste disponible, avec les conséquences décrites dans `COFFRE_0826.md`.

La lecture des banques rend la main entre les blocs déchiffrés et vérifie que la session n'a pas été verrouillée. Le format chiffré, les clés, les PIN existants et la dérivation des sauvegardes portables restent compatibles.

## Contrôles effectués

- `tools/vault-tests-0827/run_reopen.py` exécute les mêmes 32 sessions de banques altérées puis réparées sur la 0.8.26 conservée et la 0.8.27. Il compile les unités réelles et mbedTLS 3.6.5 avec AddressSanitizer ; seuls NVS, FS et les tâches sont simulés. Chaque version passe 675 assertions de conservation/accès. La 0.8.26 effectue 33 792 opérations FS supplémentaires dans les appels qui suivent le préchargement raté et expose 1 408 fois un succès pendant ce chargement. La 0.8.27 ramène ces deux compteurs à zéro. Cette contrepreuve établit le défaut logiciel, pas la cause certaine du redémarrage physique.
- `tools/credential-tests-0827/run_credentials.py` utilise les méthodes réelles de fermeture, redimensionnement, disposition et les branches coffre de l'ouverture, ainsi que la fonction OS réelle de routage du clavier et son callback de suppression du champ. Sur LVGL 9.6 avec AddressSanitizer : 13 335 assertions, 96 cycles de déverrouillage vers Clés/Identifiants/Fichier chiffré avec champ PIN actif et timers LVGL, 200 cycles de sauvegarde/fermeture, vérification de Réessayer différé et des 14 langues. Aucun accès mémoire invalide détecté sur ces parcours ; le backend et les périphériques restent simulés dans ce test UI.
- `tools/vault-tests-0827/run.py` passe 1 116 assertions sur les unités complètes avec mbedTLS : fautes NVS/FS, formats historiques, changement de PIN, récupération, absence de carte, banques de zéro octet et continuité des marqueurs worker/interface. Le résultat figure dans `tools/verification/vault-0827.json`.
- `tools/runtime-env-tests-0827/run.py` passe 3 078 assertions sur l'environnement et le shell (dont 1 000 cycles de verrouillage), les callbacks Git/libgit2 et le module Python livré. Le coffre est simulé à cette frontière ; la cryptographie est vérifiée séparément ci-dessus.
- `tools/vault-tests-0827/run_target.py` compile les quatre unités pour l'ESP32-P4 : cadre propre du worker PIN 192 octets sur sa pile de 16 Kio ; cadre maximal du cœur de chiffrement 752 octets. Ces nombres ne mesurent pas la pile cumulée SDK, le temps SD réel ni la marge matérielle.

## Si le redémarrage persiste

Un marqueur RTC dédié conserve la dernière phase observée du parcours PIN appareil : `unlock`, `auth`, `preload`, `await-ui`, `ui-rebuild`, puis `ready` une fois l'interface prête. La fermeture utilisateur termine correctement la continuation, y compris si le worker est encore actif. Le chemin API sans interface termine au retour du worker. Les marqueurs n'enregistrent aucun PIN, identifiant, secret ou adresse et n'écrivent pas sur la SD.

Après un redémarrage compatible avec la conservation RTC, le diagnostic de démarrage peut indiquer cette phase. C'est un repère pour localiser le problème, pas une preuve de sa cause. Il complète le message d'exception et la pile du journal série. Le worker historique à mot de passe n'a pas ce suivi PIN détaillé ; sa reconstruction UI reste marquée. Voir `DIAGNOSTICS_COFFRE_0827.md` pour les limites de conservation RTC.
