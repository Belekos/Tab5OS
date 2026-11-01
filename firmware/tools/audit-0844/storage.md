# Stockage — audit et corrections 0.8.44

Référence : archive livrée 0.8.43, préservée. Les tests exécutent les sources réelles du contrôleur de baux, des enveloppes `VolumeFS` et des fonctions de remplacement atomique. Les opérations Arduino/VFS sont des frontières injectées. Aucune carte physique n'a été montée, éjectée ou modifiée pendant ces essais.

## Anomalies établies

1. **Une écriture manquant d'espace pouvait déclencher une récupération du bus SD.** `tab5AtomicWrite` supprimait l'`errno` d'une écriture courte avant `flush`, puis signalait systématiquement `EIO`, y compris pour `ENOSPC`, `ENOMEM`, `EROFS` ou `EMFILE`. Le contrôleur interprétait ce `EIO` comme une erreur de communication et demandait un remontage. Le remplacement conserve maintenant la première erreur d'écriture, synchronisation, vérification de taille ou fermeture, même si le nettoyage rencontre une autre erreur. Le fichier précédent et sa sauvegarde suivent exactement la même politique de publication qu'avant.
2. **Un accès refusé pendant une récupération exclusive n'indiquait aucune erreur précise.** L'ouverture d'un chemin et l'énumération d'un répertoire pouvaient retourner un échec avec `errno == 0` ou une valeur ancienne. Ils indiquent désormais `EBUSY`. La disponibilité du média n'est pas modifiée par une simple contention.
3. **Un objet fichier partagé pouvait survivre à sa fermeture explicite sans signaler `EBADF` lors d'une lecture.** Les enveloppes fermées indiquent désormais le descripteur invalide ; le contrôleur conserve le diagnostic mais ne tente plus de réparer ce défaut de propriété par un remontage du bus. Les erreurs de transport `EIO`, `ENODEV`, `ENXIO` et `ETIMEDOUT` restent éligibles à la récupération bornée existante.
4. **L'erreur de promotion d'un temporaire pouvait être écrasée par le rollback.** `tab5AtomicReplace` restitue maintenant l'erreur initiale après avoir tenté de remettre le fichier précédent en place. Les noms des temporaires/sauvegardes et l'interface restent inchangés.

## Vérifications réalisées

| Essai hôte | 0.8.43 | 0.8.44 |
|---|---:|---:|
| Ouverture refusée par un bail exclusif : errno | 0 | EBUSY |
| Lecture d'une enveloppe fermée : errno | 0 | EBADF |
| Récupération demandée après accès fermé / EBADF | oui / oui | non / non |
| 48 erreurs d'écriture/synchronisation/taille/fermeture : cause perdue dans le retour | 48 | 0 |
| Demandes de récupération injustifiées pour erreur de ressource, sur ces essais | 32 | 0 |
| 1 000 cycles ouverture/répertoire/copie/fermeture : références SD restantes | 0 | 0 |
| Erreur de promotion ENOSPC suivie d'un rollback réussi | retour errno 0 | retour ENOSPC |

4 261 assertions dans les deux exécutables courants, avec AddressSanitizer et UndefinedBehaviorSanitizer : réussies. Les causes de transport continuent de demander une récupération avec les délais et le budget existants ; les causes de ressources ne le font pas. Les fichiers précédents sont préservés dans les 48 scénarios injectés. La détection de fuite LSan n'est pas utilisable dans cet environnement ; les références volume sont contrôlées explicitement. Aucun changement aux allocations permanentes, tâches, délais, fréquences SPI, partitionnement ou formats de données.

Reproduction :

```sh
python3 tools/storage-tests-0844/run.py --baseline-zip /chemin/Tab5OS_0.8.43_Accueil.zip
```

Sans `--baseline-zip`, le script exécute et contrôle seulement la version courante. Les sources et leurs empreintes sont consignées dans `tools/verification/storage-0844.json`. La compilation ciblée ESP32-P4 et ses empreintes figurent dans `tools/verification/storage-target-0844.json`.

## Éléments non démontrés sur matériel

Ces défauts expliquent des **demandes de récupération SD injustifiées** et des diagnostics trompeurs. Ils ne prouvent pas que toutes les indisponibilités observées après usage intensif des cartes provenaient de ces chemins. Les erreurs d'écriture et de publication propres au cache des cartes sont traitées dans l'audit Cartes.

Les baux existants excluent le remontage pendant les opérations qui les possèdent. Les références des `File` SD restent comptées jusqu'à la dernière fermeture. La configuration SDK `FATFS_FS_LOCK=0` concerne la table de conflits de fichiers ouverts, et ne suffit pas à conclure que FatFs serait dépourvu de synchronisation ; `FATFS_LINK_LOCK=1` est activé dans le SDK fourni. Aucun verrou global redondant n'a été ajouté.

Sur Tab5 : reproduire déplacement/zoom soutenu avec cache SD, puis ouverture d'un média local et de la radio. Conserver les lignes série `[SD]`, `[SD][ERREUR]`, la cause de reset et les compteurs mémoire/bloc interne maximal avant et après. Distinguer une carte réellement sans réponse (`EIO`, `ETIMEDOUT`, `ENODEV`) d'une carte pleine, d'un quota de fichiers ouverts ou d'une mémoire insuffisante. Ne pas formater ni réinitialiser les données pour ce test.
