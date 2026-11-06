# System resources and SD verification

## Recovery resources in the firmware

The firmware contains copies of the small official MicroPython modules and initial system configuration files. The release manifest and resource generator specify the actual list and sizes. After the SD card is mounted at startup, recovery recreates missing files in small steps. In **Settings > Storage**, **Restore missing files** starts the operation again and **Cancel** stops it. The screen displays progress and the number of files recreated.

Files are written to a temporary file, read back and published after verification. An existing file, even if empty or customized, is not overwritten by this recovery. Storage errors are reported; they never trigger formatting.

This recovery set does not include external fonts, ELF applications, Wikipedia archives, documents or personal data. Essential native functions remain in the firmware. For a resource that exists but is damaged, or for larger official resources, use the Factory check below after restoring its reference set to the SD card.

Without a loadable external font at `/System/font.ttf`, the interface temporarily uses English and its built-in font. The preferred language remains saved in Settings. It returns at the next startup with the font available; documents are not modified.

## Audit and conservative update

The tool checks Tab5OS resources. It does not repair the FAT structure or format the card.

The audit is read-only. It requires the `Factory/Tab5OS-0.8.53` reference set, the resources listed in its manifest and their SHA-256 checksums. This version is defined by the supplied `MicroPython/lib/tab5factory.py` module; update the module and reference set together following the [installation guide](../docs/INSTALL_EN.md). An incomplete, corrupted or different-version reference set stops the audit before any changes. This check is separate from restoring missing files from the firmware, which does not depend on Factory.

| Difference | Normal repair |
|---|---|
| Missing official file | Installed from the reference set |
| Older official file recognized by its checksum | Updated with a backup |
| User-modified script or resource | Preserved |
| Valid JSON settings or existing INI file | Preserved |
| Custom translations | Existing values preserved; missing keys added |
| Invalid JSON | Backed up in full, then replaced |
| JSON exceeding the read budget | Preserved as a modified file; not declared corrupted |
| Directory where a file is expected | Reported, never replaced |
| Document, download or other unlisted file | Ignored, never deleted |
| EIO/5 error or another communication problem | Operation stops; no deletion is inferred from this error |

Before replacement, a complete copy is created under `/.tab5-backups/repair/0.8.53/`, retaining its original path and adding a distinct numeric suffix. These backups are never cleaned up automatically. Restoring modified files is a separate option that requires confirmation; it also keeps a backup.

The plan is checked again before application; a file changed between preview and repair stops the operation. Writes use a private journal. An interruption during renaming can be rolled back using that journal. Unknown traces, inconsistent checksums or a changed destination prompt manual recovery: data is left in place. An ambiguous old journal must not be deleted to bypass this check.

These safeguards do not make a defective card or power loss during a FAT write harmless.

Neither firmware recovery nor the Factory audit restores a deleted document, repairs the FAT structure or formats storage.
