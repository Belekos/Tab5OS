# Moving from M5Writer to System

The microSD system folder is now named **`/System`**. Resources for version 0.8.24 must be copied under this name. Existing personal files must not be deleted to perform the update.

Migration runs after the microSD has been validated, before loading settings and restoring resources. It uses the native SD volume, independently of the path aliases used by applications.

| Card state | Action |
| --- | --- |
| No `M5Writer` folder and no migration in progress | No change. |
| `M5Writer` exists and `System` is absent | Rename the folder without copying its contents. |
| Both folders exist | Merge them and back up conflicts, following the rules below. |
| A migration was interrupted | Resume from `System/migration_0824/pending`. |

## Active files and backups

Files missing from `System` are moved from the old folder. When an entire subfolder is missing, it is renamed in one operation: a cache containing thousands of tiles is not copied file by file.

When two versions of the same file exist:

- **`font.ttf`, `CLAVIERS.txt`, `fonts/`, `languages/` and `ime/`** in `System` remain active. However, a personal font or language missing from `System` is added there. Conflicting old versions are retained.
- For other files, the old data from `M5Writer` becomes active. This preserves `settings.ini`, `maps.ini`, `themes.json`, `keyboards.json`, favorites and personal data in particular. The version already in `System` is backed up before this move.
- Conflicts between a file and a folder follow the same rules. No file is deleted to resolve a conflict.

After a successful merge, the backup is located at:

```text
/System/migration_0824/backup-000/
```

The `legacy` subfolder contains retained old resources and folders that have become empty; `replaced` contains versions from `System` replaced by personal data from the old folder. The number increases if a separate new migration is needed. Subsequent startups do not reread the contents of these backups.

## Failure or interruption

A destination is backed up first, then the old file is installed. If installation is interrupted between these operations, recovery sees the missing destination and completes the move. Read errors, directory-creation errors or rename errors stop migration without deleting the remaining files.

An incomplete migration is reported in the interface and serial log. Run SD recovery again once the card is readable. Do not erase `pending`: it is the source used for recovery. Copying a second `M5Writer` folder while a migration is already in progress is explicitly rejected to avoid overwriting the current migration.

The merge limits paths to 1,023 bytes and recursion to 32 levels. It does not keep one descriptor open per level: parent directories are reopened after each descent. The name `migration_0824` at the root of the old `M5Writer` folder is reserved for migration; a collision with a personal folder of that name is rejected before migration is staged.

Automated tests check errors at every operation and interruptions between operations on a simulated filesystem. They do not reproduce a power failure in the middle of a physical FAT write. Backing up the microSD remains useful before any firmware update.

## Keyboard guide filename

The official guide is now provided as `System/KEYBOARDS.txt`. The name `CLAVIERS.txt` above describes the historical migration and files already present; this migration does not automatically rename personal documents.
