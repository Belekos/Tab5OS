# License and publication status

This source snapshot does **not** declare a single license for all of Tab5OS. No project-wide license has yet been selected by the owner for the first-party files. Public source access alone must not be interpreted as an unrestricted redistribution grant.

Third-party files retain the licenses and notices in their original directories. They include permissive, GPL/AGPL and component-specific terms. In particular, the Snes9x-derived component carries non-commercial conditions. Do not replace these notices or describe the complete tree as MIT, Apache, GPL or commercially unrestricted under a single label.

The build downloads pinned external Arduino dependencies, including ESP8266Audio. The 0.8.53 linked firmware contains Helix AAC code under RPSL together with GPLv3 audio code. The publication audit found no exception covering this combination. This issue needs clarification with the relevant rightsholders or a technically validated compatible implementation before releasing a combined firmware binary. Choosing a license for Tab5OS's original files does not by itself resolve it.

This repository therefore contains **sources and publication documents, not precompiled firmware or ELF applications**. No decoder has been removed or replaced as part of preparing the repository.

See the [third-party notices](firmware/LICENCES.md) and [third-party manifest](firmware/LICENCES-manifest.json). The notices in each component directory remain authoritative for that component.

## Français

Aucune licence principale n’a encore été choisie pour les fichiers propres à Tab5OS. Les sources tierces gardent leurs notices et leurs conditions, dont certaines ne sont pas permissives. L’accès public au code ne vaut pas autorisation générale de redistribution.

L’association réellement liée de Helix AAC sous RPSL avec du code audio GPLv3 doit être clarifiée avant diffusion du binaire combiné. Le dépôt est donc préparé sous forme de sources et de documentation, sans firmware ni ELF précompilé. Le code audio stable n’a pas été modifié pour contourner ce point.
