# Coffre 0.8.26 : validation native et cible

`python run.py` compile les sources de production KeyStore, Vault, VaultCore, Secrets et Environment, liées aux objets mbedTLS 3.6.5 réels. Seuls FS, NVS, tâche et aléa matériel sont simulés. Les tests reprennent la couverture .25 et ajoutent : PIN initial obligatoire, ajout de PIN sans remplacement du maître, réauthentification pour modification/retrait, ouverture asynchrone révocable, récupération explicite avec 40 interruptions NVS/SD, conservation exacte des anciens fichiers chiffrés et refus d'accès par variables lorsque le PIN manque ou les banques sont corrompues.

`python run_target.py` compile les quatre unités coffre pour ESP32-P4 avec les vrais en-têtes SDK et conserve les cadres de pile. Les piles cumulées SDK et les marges physiques ne sont pas mesurées.

Rapports : `../verification/vault-0826.json` et `vault-target-0826.json`. Les sources et tests historiques .25 sont conservés dans leur dossier initial. Les limites matérielles et conséquences de récupération sont détaillées dans `../../COFFRE_0826.md`.
