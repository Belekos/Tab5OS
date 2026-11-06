# MicroPython libraries and projects

Open **Code → Libraries**, **Workshop → Libraries**, or the MicroPython script `libraries.py`:

- **SD catalog**: find libraries by board family and purpose.
- **Import SD / USB**: copy a `.py` module, an `.mpy` module or an extracted folder already on the SD card or USB drive.
- **Get sources over Wi-Fi**: download a mip package, an HTTPS address or a `github:` source, with its mip dependencies. The sources remain available offline afterward.
- **New MicroPython project**: create a folder containing `main.py` and a target description.
- **Installed modules for Tab5**: access the existing module manager for scripts running on this Tab5.

## Organization and use

The catalog uses `/MicroPython/Bibliotheques/<carte>/<usage>/<nom>/lib/`. Classification information is stored in `metadata.json` beside the files. Neither upstream package metadata nor sources are executed to determine compatibility. The catalog is never added to the global Python search path.

Available targets are: portable / to be checked, Tab5 P4, ESP32, S3, C3, C6, P4 on another board, ESP8266, RP2040, RP2350, STM32 and other board. Available purposes are: general, sensors, display, networking, storage, motors and audio. This information is a **user-selected declaration**, not a hardware test result.

Importing a folder preserves its directory structure. If the folder contains `__init__.py` or `__init__.mpy`, its name is kept as the Python package name. Otherwise, its contents are placed directly in `lib/`. Choose the package folder or the contents of the `lib` folder, not the entire repository root. Ordinary supporting files are preserved; hidden folders, `__pycache__` and `node_modules` are ignored. ZIP archives must be extracted before import. Dependencies of a local import must be included in the folder or added separately.

Within a library, **Add to project** lets you create a project or choose an existing folder on SD. A USB folder is rejected before any write: the transaction journal resides on SD. USB remains available as an import source. Sources and their directory structures are copied into the project's `lib/` subfolder. An identical copy is reused; a differing file blocks the entire addition before any replacement. The project's personal source files are preserved. Files modified in the catalog since import also block the addition: import them again under another name to keep both variants.

New projects are placed in `/MicroPython/Projets/<nom>/main.py`. Open this file in **Writing**; to add it to the editor's projects, use **Documents → Import folder**. The `.tab5-project.json` description stores only the declared target and added libraries. It does not change the external board's settings.

## Portability and limits

A `.py` source may depend on a module, peripheral or pin assignment unavailable on the target. Declaring a target does not validate any of these. An `.mpy` file is preserved without execution: only its minimal header is checked on import; the MicroPython version, firmware settings and, for native code, the architecture must still be checked on the destination board. The manager does not convert `.mpy` files. mip downloads use `.py` sources.

This workflow prepares real files and projects on SD **without a computer**. Transferring these files to a board, flashing its firmware and launching the program are not included in this feature. Running a script in Tab5OS runs it on the Tab5, regardless of its folder's declared target. The VM can load modules from the script's `lib` subfolder; Tab5 system modules retain priority.

To preserve memory and responsiveness, a local import is limited to 160 files, 1 MiB per file, 8 MiB total and 8 directory levels. Traversal uses only one of the native bridge's eight directory descriptors and yields regularly. Copies use 4 KiB blocks. The browser loads classification records without loading sources or retaining their hash tables. If more than 256 libraries are displayed, filter by target or purpose. mip downloads retain their limits of 32 packages, 128 source files and 4 MiB.

Operations use the existing recovery journal: an interrupted copy or commit should restore the previous files. Internal download temporary files are now recognized by recovery; an interruption during a download should no longer block every Workshop operation.
