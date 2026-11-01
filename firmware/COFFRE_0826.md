# Coffre — corrections 0.8.26

Le premier accès demande un PIN de 4 à 12 chiffres et sa confirmation. Un coffre appareil créé sans PIN avec une ancienne version demande également d'en définir un. Le changement de PIN exige le PIN actuel, même si le coffre est déjà ouvert. L'interface ne propose plus de retrait du PIN. Les anciens coffres à mot de passe restent lisibles et migrables.

Le bouton du gestionnaire s'appelle **Copier**. Le presse-papiers protégé conserve son expiration interne de 30 secondes et est effacé au verrouillage. Les valeurs masquées ou révélées restent exclues de l'historique annuler/rétablir.

## Ouverture et stabilité

L'ouverture PIN, la lecture NVS et le premier déchiffrement des deux banques d'identifiants s'exécutent sur une tâche dédiée à pile interne de 16 Kio. Le callback LVGL copie au plus deux PIN de 12 chiffres, lance la tâche et efface les champs. Aucun objet LVGL n'est utilisé par la tâche. Le remplacement des pages intervient dans le service de l'application, après le retour du callback. Les copies de trousseau utilisées par la création et la suppression de clés sont également sorties de la pile UI.

Les fonctions cibles compilées ont des cadres statiques de 192 octets pour le worker appareil, 752 au maximum pour le cœur de chiffrement et 1 824 pour l'ancien worker à mot de passe. Cela ne mesure pas la pile cumulative des appels SDK, ni la marge réelle sur tablette. La tâche imprime sa marge minimale à sa fin. Un essai matériel reste nécessaire pour confirmer la disparition du redémarrage signalé.

## Coffre inaccessible

L'écran verrouillé propose **Coffre inaccessible ?**. Cette page précise les conséquences et exige de saisir littéralement `RESET` avant l'action **Réinitialiser le coffre**. Cette action ne demande pas le PIN oublié.

Elle supprime le trousseau appareil et son PIN (`device3`, `pin4` dans l'espace NVS `tab5keys`). Les fichiers chiffrés ordinaires, sauvegardes de clés portables et l'ancien coffre à mot de passe ne sont pas supprimés. Le dossier `/System/Secrets` est renommé vers un dossier unique `/System/Secrets-retired-…` avant la suppression des clés. Les identifiants y restent chiffrés et deviennent inaccessibles après cette opération ; leur conservation n'est pas une promesse de récupération sans le maître supprimé. Les fichiers chiffrés avec les clés de fichiers restent récupérables uniquement avec la sauvegarde de la clé correspondante.

Une marque NVS `reset4` bloque l'ouverture pendant une récupération interrompue. Répéter la même action reprend l'archivage/suppression ; aucun ancien dossier de récupération n'est écrasé. La carte SD doit être disponible. Une erreur de carte ou d'écriture n'efface pas les clés avant l'archivage. Une nouvelle utilisation impose un nouveau PIN et crée un coffre vide.

## Vérification

- `tools/vault-tests-0826/run.py` : unités de production complètes avec mbedTLS 3.6.5 et environnement de variables réels ; FS/NVS/RTOS simulés. 1 097 assertions, 95 erreurs injectées sur l'écriture des identifiants, 40 positions d'erreur sur la récupération, 50 cycles sauvegarde/réouverture. L'ajout d'un PIN à un coffre .25 conserve exactement le maître, l'identifiant et les identifiants enregistrés. Les accès par variables restent fermés avant création du PIN et après corruption des banques. Les preuves .25 sont conservées séparément.
- `tools/vault-tests-0826/run_target.py` : compilation ESP32-P4 et cadres de pile des quatre unités.
- `tools/credential-tests-0826/run_credentials.py` : méthodes UI de production, vrai LVGL 9.6 et TextInput, backend simulé ; PIN obligatoire, réauthentification, récupération volontaire, transitions différées, 200 cycles sauvegarde/fermeture et géométrie dans les 14 langues.

Les formats chiffrés existants, la dérivation PBKDF2 portable à 600 000 tours et les sauvegardes locales demeurent compatibles. Aucun test logiciel ne remplace un essai de coupure d'alimentation, de retrait physique de carte ou d'extraction matérielle de la flash. Le PIN est un contrôle d'accès local ; le maître reste en NVS interne.

