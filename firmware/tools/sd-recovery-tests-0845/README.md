# Contrôleur de reconnexion SD — tests 0.8.45

Exécuter depuis n'importe quel répertoire :

```sh
python3 tools/sd-recovery-tests-0845/run.py --baseline-zip /chemin/Tab5OS_0.8.44_Medias_Cartes.zip
```

La fixture utilise les bibliothèques FS/VFS complètes épinglées, les enveloppes VolumeFS et les baux de production, la fermeture réelle de pack Cartes et les trois fonctions privées complètes du sketch. Les classes d'applications et l'effet électrique des opérations SD/SPI sont des frontières adaptées ; aucun essai physique n'est simulé comme une réussite matérielle.

`before-owned-result.inc` conserve exactement les trois méthodes privées du contrôleur juste avant la correction locale du résultat d'erreur. Il ne correspond pas à une version de firmware livrée. Il permet de reproduire séparément la course où un worker refusé pendant l'exclusivité journalise EBUSY après le probe EIO, puis le contrôleur utilisait ce diagnostic global comme résultat de sa propre opération :

```sh
python3 tools/sd-recovery-tests-0845/run.py --probe-race-before-fix
```

Ce second mode réussit lorsqu'il retrouve le défaut attendu du fragment historique ; il ne produit pas le rapport de validation courant. Le mode normal vérifie la correction : une vraie ouverture d'un autre thread est refusée par le vrai VolumeFS, le diagnostic global devient EBUSY, mais la cause EIO du probe conserve sa reprise de transport. Les rapports distinguent les erreurs de transport des erreurs non réparables par remontage.

ASan/UBSan actifs, LSan indisponible. Les erreurs POSIX sont injectées dans les appels réels du SDK et les références sont contrôlées explicitement. Les sources et les fixtures utilisées sont hachées avant et après l'exécution.
