# Repository layout

| Path | Purpose |
| --- | --- |
| `README.md`, `README.fr.md` | Project introduction in English and French |
| `docs/INSTALL_EN.md`, `docs/INSTALL_FR.md` | First-install versus OTA update instructions |
| `docs/BUILD.md` | Current build commands and partition-table clarification |
| `docs/screenshots/` | Actual LVGL host renders, covers and provenance |
| `docs/publication/` | Source manifest and bounded publication/license findings |
| `firmware/Tab5OS_Arduino_v0.8.53_Ecriture/` | Original sketch, native components, Python source and small SD resources |
| `firmware/tools/` | Build, resource generation and test scripts |
| `firmware/*.md` | Original engineering and application documentation, including historical notes |
| `NOTICE.md` | License boundaries and current source-only publication status |

The versioned Arduino sketch name is intentional. Keeping the original relative layout under `firmware/` avoids changing includes, scripts or application build paths merely for publication.

Every copied file is recorded with its archive path, repository path, byte size and SHA-256 in [source-manifest.json](publication/source-manifest.json). Top-level documentation and the gallery are new publication additions; they are not presented as part of the original compiled firmware.

Omissions from Git include compiled BIN/ELF/debug artefacts, duplicate `sdcard/Factory` payloads, large generated city/input resources and bulky generated historical logs/captures. Their names, sizes and reasons are recorded in the same manifest. The original delivery is preserved separately. This omission is repository packaging, not removal of functionality from the firmware code.

Current JSON test summaries are included as records of the original 0.8.53 qualification. Full archive-publication verification depends on the original complete evidence and resource payloads; it is intentionally distinct from building this source tree. Build outputs and user credentials are excluded by `.gitignore`.
