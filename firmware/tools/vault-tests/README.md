# Coffre 0.8.25 : preuves et limites

`python run.py` compile les unités de production complètes `Tab5KeyStore`, `Tab5Vault`, `Tab5VaultCore` et `Tab5Secrets`. La cryptographie utilise les objets mbedTLS 3.6.5 natifs, provenant du même commit que le SDK ESP-IDF 5.5.4. FS, NVS, RTOS et générateur aléatoire sont remplacés par des frontières déterministes. La crypto n'est pas simulée. Les empreintes des sources, du test et des objets crypto sont inscrites dans `../verification/vault-0825.json`.

Les contrôles portent sur :

- PBKDF2-SHA-256 à 600 000 itérations, comparaison octet pour octet à l'implémentation officielle, annulation pendant la dérivation et effacement du résultat d'erreur ;
- accès PIN, délais d'essais, absence de contournement par `openDevice`, retrait du PIN après ouverture ;
- sauvegarde locale, suppression de la clé initiale, restauration et réouverture après les tâches ;
- export portable, mauvais mot de passe, restauration et compatibilité du format existant ;
- maître du coffre de mots de passe indépendant des clés de fichiers, noms/valeurs chiffrés, effacement des buffers de sortie en erreur, capacité 64 entrées × 512 octets ;
- 95 positions d'erreurs FS sur la publication d'un nouvel état, relecture après invalidation du cache et conservation de l'ancien état lorsqu'une mise à jour échoue ;
- altération de l'en-tête et des blocs GCM sans publication de clair, refus de deux banques de mots de passe corrompues ;
- migration de l'ancien trousseau, changement du mot de passe historique, collision d'identifiant et fusion vers un appareil déjà protégé par PIN ;
- création de tâche refusée et nouvelle tentative après erreur.

`python run_target.py` compile les mêmes unités avec le compilateur ESP32-P4 et les vrais en-têtes du SDK. Il conserve les cadres `-fstack-usage` par fonction dans `vault-target-0825.json`. Il ne mesure pas les appels internes mbedTLS/NVS ni le maximum de pile réellement atteint sur tablette. La comparaison avant/après porte sur les enveloppes C++ du dépôt : ouvrir le coffre passe de 2864 à 192 octets, persister de 2016 à 112, les enveloppes temporaires étant désormais allouées puis effacées.

Les tests ne prétendent pas établir la latence réelle, l'endurance NVS, le comportement d'une carte SD physiquement retirée, la tenue à une coupure électrique réelle ou la sécurité face à l'extraction matérielle de la flash. Le rapport de test est une preuve de simulation logicielle et de cryptographie native, pas un essai matériel.
