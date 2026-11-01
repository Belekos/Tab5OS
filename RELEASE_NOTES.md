# Tab5OS 0.8.53

## Changes

- Explorer menus now use the shared pointer-hover style while preserving keyboard focus.
- Image quick actions distinguish Viewer and Drawing, with more space above the first file row.
- The alarm icon is redrawn.
- Meshtastic can explicitly open the virtual keyboard with a USB keyboard attached.
- Home entries use the shorter labels **Apps ELF** and **Apps Python**.
- The sidebar supports built-in, ELF and Python applications, with up to ten shortcuts.
- EPUB pagination storage is allocated when needed; immutable JPEG and tuner tables use Flash.

Existing saving and autosave choices are preserved.

## Repository update

The presentation and installation guides are simplified. The Arduino sketch is named `Tab5OS_Arduino_v0.8.53`, without the former `_Ecriture` suffix. Historical reports and the former `firmware/tools` directory are removed; the compilation instructions describe the retained build helpers.

External ELF and Python application sources remain included. Firmware binaries and a complete SD installation package are separate deliverables and are not yet published here. This repository update does not introduce a new firmware version.

The gallery uses LVGL renders on a computer. No new physical-device or M5Burner validation is claimed.
