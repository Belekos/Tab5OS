# Coffre — Tab5OS 0.8.28

Le retour matériel sur la 0.8.27 signale encore un redémarrage à l’ouverture des clés ou du gestionnaire de mots de passe, ainsi qu’au lancement de la réinitialisation. Ces parcours n’ont pas été reproduits sur une tablette physique pendant le développement.

## Corrections

Le correctif commun de synchronisation cryptographique empêche une vérification ECDSA de perturber une opération SHA/AES concurrente avec le SDK ESP-IDF 5.5.4 livré. Le coffre et les connexions TLS utilisent ces services partagés. La contrepreuve logicielle démontre l’absence de cette exclusion dans le SDK et son rétablissement ; elle ne démontre pas que tous les resets observés ont cette cause. Voir [CRYPTO_DIAGNOSTICS_0828.md](CRYPTO_DIAGNOSTICS_0828.md).

Les tampons de chiffrement du coffre utilisent désormais un alignement PSRAM de 128 octets et un espacement adapté entre entrée et sortie. Leur effacement avant libération et le refus d’un contenu altéré restent contrôlés. Le pilote SDK possède déjà des copies intermédiaires pour certaines entrées non alignées : cette amélioration de disposition n’est donc pas présentée comme la cause établie du redémarrage.

Les actions et champs incompatibles avec un travail en cours sont désactivés. Le bouton de presse-papiers porte le seul libellé **Copier** ; son expiration interne et son effacement au verrouillage sont conservés. Les protections existantes restent en place : PIN obligatoire au premier accès, PIN actuel pour le modifier, suppression de clés confirmée, réinitialisation explicitement destructive avec saisie de `RESET`.

Le chargement des identifiants conserve le comportement de la 0.8.27 : un échec reste mémorisé pour la session et ne déclenche pas de nouvelles lectures SD ou opérations de déchiffrement depuis l’interface. **Réessayer** lance une tentative explicite dans le worker. La réinitialisation ne récupère ni un PIN oublié ni une clé perdue.

## Diagnostic et preuves

Les phases du coffre sont détaillées dans le contexte RTC. Après un redémarrage permettant sa conservation, consulter **Réglages → Système → Informations système → Avant le reset (contexte)**. Ces phases indiquent le dernier travail observé. Aucun PIN, mot de passe, identifiant, nom de fichier ou URL n’est enregistré dans les détails du coffre ou de Git, et ces marqueurs n’écrivent pas sur la SD.

Les interfaces NVS, fichiers et RTOS nécessaires aux essais sur ordinateur sont simulées. Les tests ne mesurent pas la pile libre ni l’alimentation réelle de la tablette. Le test physique reste nécessaire pour confirmer le retour utilisateur.
