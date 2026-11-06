# Maps: viewing and offline preparation

In 0.8.45, search prefers the Internet when the connection and transport are available, then falls back to local data after a failure or an empty result. Without Wi-Fi, or while a radio holds an exclusive network lease, local search remains directly available.

Zooming and panning remain possible while tiles load. Requests for the previous view are canceled so the new view can load.

The slider now offers display zoom up to **22** for the supplied sources. While dragging the slider, only the indicated level changes; loading begins on release. Keyboard arrows on the slider also apply the change. On the map: arrow keys pan, `+`/`-` zoom, `Home` returns to the home location, `I` shows information at the center, and `Escape` returns to the controls.

## Startup position and resuming the view

On a tablet with no Maps preferences, the initial view is a neutral world view: latitude 0°, longitude 0°, zoom 2. No personal position is supplied.

In **Maps → Map settings** (map icon), **Remember last view** is enabled by default. The position and display zoom are saved when leaving Maps for another page. They are restored on the next opening, including after a restart. Zoom remains the display zoom (up to 22); the engine always chooses a tile level actually available from the source.

When this option is disabled, opening Maps returns to the home position explicitly saved in **Saved places**, or to the world view if no home has been set. Enabling the option again saves the current view on the next exit. Setting a home position does not replace the last remembered view. Older positions saved on the tablet are read if complete and valid; no position data is imported from the PC used to build the firmware.

Resuming uses a single **24-byte record in internal NVS memory**, independent of the microSD. Nothing is saved during panning, zooming or refreshes. Leaving and reopening without changing the view does not rewrite this record. An unreadable, incomplete or out-of-range value is ignored; a save failure is reported on screen.

A forced power cut while Maps remains open cannot save the latest movement: the next opening resumes from the last successful exit. Leave Maps before switching off to preserve the displayed view.

## Display zoom and available detail

| Supplied source | Highest tile zoom | Maximum display zoom | Magnification at maximum |
| --- | ---: | ---: | ---: |
| OpenStreetMap | 19 | 22 | ×8 |
| OSM light | 19 | 22 | ×8 |
| OpenTopoMap | 17 | 22 | ×32 |

Beyond the source's highest level, the map enlarges parent tiles. This makes them easier to read without inventing new detail. OSM light continues to use a lower level where possible. Magnification is limited to five levels; it does not create a giant image in memory: rendering calculates only the pixels needed in the existing canvas.

Zoom is preserved with the last view, home position and saved places. Existing `.t5map` packages and XYZ caches remain compatible.

## Point information

Enable the **?** button, then touch the map without dragging it. The window shows latitude/longitude, display zoom, source zoom, approximate meters per pixel, source, and the number of local packages for that source. A saved place appears if it is within 48 pixels of the chosen point.

The window lets you center the map or save that place. `I` opens information at the center from the keyboard; `Tab` moves between actions and `Escape` closes the window. Enabling information mode does not prevent dragging the map.

This view sends no requests. Raster tiles do not contain building, address or business records: that information is not inferred from the image. The package count applies to the source and does not guarantee that every package covers the chosen point.

## Prepare an area on a PC or on the tablet

Both importers now accept a display zoom range of **0–22**. They convert it to the levels actually available before estimating, downloading or building packages. For example, an area requested only at z22 imports z19 tiles for OSM, or z17 for OpenTopoMap. The viewer enlarges them in exactly the same way online and offline.

On the tablet, frame the area, then touch the download icon: the native **Download area** window contains the source, both zoom levels and request rate. No MicroPython program is required. The rectangle visible when the window opens remains the import boundary. The tile count is calculated immediately; above 50,000 tiles, reduce detail or frame a smaller area. On a PC, the area remains a circle defined by its center and radius. The PC tool is optional.

### Request rate

- **PC**: `tools/Cartes_Tab5.cmd`, **Download speed** section, then the "Requests per second" field, retained between sessions. Range: **0.1 to 100 requests/s**, with one shared limit across all connections. Command line: `--requests-per-second 0.5`.
- **Tablet**: Maps → download icon → **Requests per second** slider. Range: **0.1 to 4**; the value is saved in device preferences. The source's effective limit is displayed beside the slider.
- **Public OpenTopoMap**: the limit remains **0.5 requests/s**, meaning at least two seconds between requests, even if a higher value is entered. A lower rate can always be selected.

This is a maximum rate, not a guaranteed speed. Network time is added to the delays; the server and connection may slow the import. Pause the native import before changing its rate, then resume. The setting is saved when releasing the slider or changing it with the keyboard.

PC retries also respect the configured delay. The native import pauses on error and requires explicit resumption: a 403 or 429 refusal is never followed by an automatic retry. Delays can be canceled and the interface remains available. During a transfer already in progress, Pause takes effect when it finishes or reaches its network timeout. On a PC, Pause no longer kills the tile-import process.

Each new tile is checked and published in the cache before continuing. A failed transfer preserves completed files. Pause, Close and Escape cancel the remaining requests. To resume, reopen the same area with the same zoom levels: cached tiles are skipped. The tablet keeps at most one pending tile, up to 512 KiB, and 23 coordinate rectangles; no list grows in proportion to the number of tiles. Downloads use the existing Maps worker, memory reservations and OS network supervision. The older Python import application remains usable separately.

## Sources and usage terms

The standard `tile.openstreetmap.org` server is reserved for interactive viewing and prohibits preloading areas. The importers retain this restriction. For offline OSM, use the supplied local PC renderer or a source that permits this use. A custom request rate does not remove source conditions or application limits.

- OSM policy: <https://operations.osmfoundation.org/policies/tiles/>
- OpenTopoMap source, attribution and service status: <https://opentopomap.org/about>
- Complete PC guide: `Tab5OS_Arduino_v0.8.53/tools/CARTES_PC.md`.

## Information and search

The i button shows information at the center; a long press on the map chooses a point. Search retains eight results, filters the useful JSON data and cancels abandoned requests. Allocations and software checks are described in [CARTES.md](CARTES.md); confirmation that the restart issue is resolved still requires testing on the tablet.
