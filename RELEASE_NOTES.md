# Tab5OS 0.8.53 — source publication

This repository snapshot starts from the authenticated `Tab5OS_0.8.53_Explorateur_Bandeau.zip` delivery, SHA-256 `a6c24e92dabb36b4eac18df4c9cff9c63b3f53d0546a54dfd863439fcc56ed58`.

## Functional changes in 0.8.53

- Explorer menu hover uses the shared pointer style while preserving keyboard focus behavior.
- Image quick actions distinguish Viewer and Drawing; Drawing is available directly, and the first file row is separated from the actions by 8 pixels.
- The alarm icon is redrawn.
- Meshtastic can explicitly open the virtual keyboard even with a USB keyboard attached.
- Home labels are shortened to **Apps ELF** and **Apps Python**.
- The sidebar supports native, ELF and Python applications, with stable identifiers and up to ten shortcuts without scrolling.
- EPUB pagination storage is allocated when needed, preferably in PSRAM; immutable JPEG/tuner tables are stored in Flash.

The verified build reduced static internal DATA+BSS by **2,620 bytes net**. Partitions, IRAM, TCM and RTC sections remained unchanged. The existing choices on file saving and autosave were preserved.

## What this repository preparation changes

Only publication layout, new documentation and a gallery are added. Included firmware and build-source files remain byte-identical to the authenticated archive. No new firmware version is claimed. Precompiled binaries, bulky historical evidence and duplicate/generated SD payloads are omitted from Git and listed in the [source manifest](docs/publication/source-manifest.json).

Current build summaries are retained in `firmware/tools/verification/*-0853.json`; these describe the original verified release and reference detailed evidence not all included in this source-only tree. The complete historical package is not republished here while binary distribution is under license review.

## Publication status and limitations

No firmware download or M5Burner listing is claimed available by this source snapshot. The first-party license decision and the RPSL/GPL audio combination require resolution; see [NOTICE.md](NOTICE.md).

The gallery contains computer-rendered LVGL previews, not physical-device captures. No new hardware validation was performed. The full functional report and device checklist are retained in [firmware/RAPPORT_FONCTIONNEL_0.8.53.md](firmware/RAPPORT_FONCTIONNEL_0.8.53.md) and [firmware/TESTS_TAB5_0.8.53.md](firmware/TESTS_TAB5_0.8.53.md).
