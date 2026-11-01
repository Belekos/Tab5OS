# Passage de M5Writer à System

Le dossier système de la microSD s’appelle désormais **`/System`**. Les ressources
de la 0.8.24 doivent être copiées avec ce nom. Les anciens fichiers personnels
ne doivent pas être supprimés pour effectuer la mise à jour.

La migration s’exécute après validation de la microSD, avant le chargement des
réglages et la restauration des ressources. Elle utilise le volume SD natif,
indépendamment des alias de chemins utilisés par les applications.

| Situation sur la carte | Action |
| --- | --- |
| Ni `M5Writer` ni migration en cours | Aucun changement. |
| `M5Writer` existe et `System` est absent | Un renommage de dossier, sans copier son contenu. |
| Les deux dossiers existent | Fusion avec sauvegarde des conflits, selon les règles ci-dessous. |
| Une migration a été interrompue | Reprise du dossier `System/migration_0824/pending`. |

## Fichiers actifs et sauvegardes

Les fichiers absents de `System` sont déplacés depuis l’ancien dossier. Lorsqu’un
sous-dossier entier manque, il est renommé en une seule opération : un cache de
milliers de tuiles n’est pas recopié fichier par fichier.

En présence de deux versions du même fichier :

- Les fichiers **`font.ttf`, `CLAVIERS.txt`, `fonts/`, `languages/` et `ime/`** de
  `System` restent actifs. Une police ou langue personnelle absente de `System`
  y est cependant ajoutée. Les anciennes versions en conflit sont conservées.
- Pour les autres fichiers, les anciennes données de `M5Writer` deviennent
  actives. Cela conserve notamment `settings.ini`, `maps.ini`, `themes.json`,
  `keyboards.json`, les favoris et les données personnelles. La version déjà
  présente dans `System` est sauvegardée avant ce déplacement.
- Les conflits entre un fichier et un dossier suivent les mêmes règles. Aucun
  fichier n’est effacé pour résoudre le conflit.

Après une fusion réussie, la sauvegarde se trouve dans :

```text
/System/migration_0824/backup-000/
```

Le sous-dossier `legacy` contient les anciennes ressources conservées et les
dossiers devenus vides ; `replaced` contient les versions de `System` remplacées
par les données personnelles de l’ancien dossier. Le numéro augmente si une
nouvelle migration distincte est nécessaire. Les démarrages suivants ne relisent
pas le contenu de ces sauvegardes.

## Échec ou interruption

Une destination est d’abord sauvegardée, puis l’ancien fichier est installé.
Si l’installation est interrompue entre ces deux opérations, la reprise voit la
destination absente et termine ce déplacement. Les erreurs de lecture, de
création de dossier ou de renommage arrêtent la migration sans supprimer les
fichiers restants.

Une migration incomplète est signalée dans l’interface et le journal série.
Relancer la récupération SD une fois la carte de nouveau lisible. Ne pas effacer
`pending` : c’est la source permettant la reprise. Un second dossier `M5Writer`
recopié pendant qu’une migration est déjà en cours provoque un refus explicite,
afin de ne pas écraser cette dernière.

La fusion limite ses chemins à 1 023 octets et la récursion à 32 niveaux. Elle
ne garde pas un descripteur ouvert par niveau : les parents sont rouverts après
chaque descente. Le nom `migration_0824` à la racine de l’ancien `M5Writer` est
réservé au mécanisme de migration ; une collision avec un dossier personnel de
ce nom est refusée avant la mise en attente.

Les essais automatisés vérifient les erreurs de chaque opération et les
interruptions entre opérations sur un système de fichiers simulé. Ils ne
reproduisent pas une panne électrique au milieu d’une écriture FAT matérielle.
Une sauvegarde de la microSD reste utile avant toute mise à jour de firmware.

## Nom de la notice des claviers en 0.8.38

La notice officielle est désormais fournie sous `System/KEYBOARDS.txt`. Le nom `CLAVIERS.txt` ci-dessus décrit la migration historique et les fichiers déjà présents ; cette migration ne renomme pas automatiquement les documents personnels.
