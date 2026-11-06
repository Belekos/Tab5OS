# OpenStreetMap and OpenTopoMap for Tab5OS

This program prepares a complete area on the PC and copies it to the SD card. It offers only **OpenStreetMap** and **OpenTopoMap**, using cache identifiers recognized by Tab5OS.

## Getting started on Windows

Install Python 3.10 or later with Tkinter (included in the official Windows installer), then open **`Cartes_Tab5.cmd`**. No additional Python packages are required. The PC tool currently uses French interface labels; their English meanings are given below.

1. Choose **OpenTopoMap** to download directly, without an account or Docker. This is the initial choice; an already saved preference is preserved.
2. Search for a city and select the correct result, or enter latitude/longitude.
3. Choose the radius, for example **100 km**, then zoom levels. Start with **0 to 12** to assess the size.
4. Choose a PC directory and, optionally, the SD card root.
5. Under **Download speed** (`Vitesse de téléchargement`), set **Requests per second** (`Requêtes par seconde`, 0.1-100).
   The effective rate and source limit are displayed immediately.
   Your choice is retained between sessions.
6. Click **Estimate** (`Estimer`), then **Download / resume** (`Télécharger / reprendre`).

This PC tool is optional. On the tablet, the Maps download icon directly opens a native import of the displayed area, with its own rate slider (0.1-4 requests/s). It does not open a Python application.

The calculation covers tiles intersecting the circle, including near the antimeridian. It is not limited to currently visible tiles. Each additional zoom level can roughly quadruple the detailed tile count. Estimates use 32 KiB per tile, but each PNG's content varies. The PC keeps both cached PNGs and packages, using roughly twice the stated map size. The SD card receives only the packages.

**Pause** keeps completed PNGs. Running exactly the same area again resumes using these files after verification. Changing the source or area requires a new package name to avoid mixing data. An incomplete download is never reported as a completed area.

Packages are published only after writing is complete and are split into volumes of **at most 1,900 MiB by default** (configurable limit: 2,000 MiB). This size allows the current firmware to read every file completely; the SD card's total capacity may be much larger. There may therefore be several `.t5map` files. All are required. Tab5OS reads up to **128 packages in this directory, across all maps**. The program rejects an area that would exceed this limit and checks the package count before copying to SD; old unchanged volumes being replaced are included in that calculation. Also retain the generated `.t5map.manifest.json`: subsequent exports use it to remove only their own old, unchanged volumes. Personal, modified or old files without a manifest are preserved. If no SD card was selected, copy the volumes and manifest into:

```
System/maps/offline/
```

If an old package exceeds 2 GiB, rebuild it from the PC cache with this version of `build_t5map.py`, or rerun the same download command: completed PNGs are reused without downloading them again. Splitting accounts for both data and indexes.

On Tab5, open **Maps** and choose **OSM Standard** or **OpenTopoMap**. Packages are read directly; no conversion or SQLite database is loaded into RAM. Keep the PC cache if you want to resume or rebuild packages. Deleting it later does not remove maps from the SD card.

## OpenTopoMap: direct source, moderate rate

Leave **XYZ URL** (`URL XYZ`) blank: the program directly uses `https://a.tile.opentopomap.org/{z}/{x}/{y}.png`, up to zoom 17. A custom URL stays associated with the selected map; switching from OSM to OpenTopoMap no longer accidentally reuses the OSM server address. The project allows its map to be integrated provided its server is not overloaded, particularly through bulk downloads. It does not publish a quota guaranteeing that an area of any size will be accepted.

The program therefore enforces **one connection and at most one tile every two seconds** on this public server. It displays the corresponding minimum duration before downloading; server response time adds to that minimum. For example, Paris with a 100 km radius and zoom levels 0 to 14 represents 16,735 tiles, about 523 MiB at 32 KiB/tile and a minimum of 9 h 18 min without cached tiles. A detailed area may take several days. On HTTP 403 or 429, it stops and retains resume data; it does not switch subdomains to bypass the refusal. This conservative rate is neither permission for unlimited downloads nor a guarantee of availability.

For very large areas, enter a personal or explicitly authorized server in the URL field. The [OpenTopoMap project](https://github.com/der-stefan/OpenTopoMap) publishes its style and instructions for generating your own tiles; this topographic rendering also requires elevation data and is more complex than the OSM setup supplied here. Garmin files are not PNGs and are not directly compatible with Tab5OS.

The OpenTopoMap project has announced a future transition to vector tiles. This program uses the raster PNGs still served; future vector files will not be directly readable by Tab5OS. Packages already copied to SD remain usable offline.

Attribution remains visible on the tablet: data © OpenStreetMap, SRTM; map © OpenTopoMap, CC-BY-SA.

## OpenStreetMap: the same map, rendered on the PC

The public `tile.openstreetmap.org` server prohibits preparing offline areas. This restriction depends on the server, not on whether a tablet or PC is used. OSM data itself is available as regional downloads. The supplied solution uses that data and the actual **OpenStreetMap Carto** style through the open-source `overv/openstreetmap-tile-server` project.

Install and start **Docker Desktop with Linux containers**, then choose **Prepare OSM on this PC** (`Préparer OSM sur ce PC`). Select an already downloaded `.osm.pbf` file or its HTTPS URL from [Geofabrik](https://download.geofabrik.de/). Choose a region covering **the entire requested circle**, using a country extract if the area spans several regions. The renderer cannot invent roads outside the extract.

Before importing, the program displays the planned resources and asks for confirmation: up to **12 GiB RAM**, **4 threads**, and at least **40 GiB free disk space in Docker storage** for a small extract; a large region needs more. Importing and initial renders can take a long time. A GPU is not required. Additional components are downloaded during the first import; local rendering then works without Internet access.

After importing, the server listens only on the PC:

```
http://127.0.0.1:8080/tile/{z}/{x}/{y}.png
```

This is the program's default OSM source. Rendering low-zoom tiles may take more than a minute for large regions; if needed, command-line mode accepts `--timeout 300`. The map uses OpenStreetMap Carto with data from the selected extract; its appearance may vary slightly with the style revision in the Docker image.

Preparation never destroys an existing database. For a new extract or update, use a new instance name:

```powershell
py setup_osm_server.py --pbf "C:\Cartes\region.osm.pbf" --instance region-2026-09
```

To inspect commands without running them, add `--plan-only`. To stop the server and free RAM, use the `docker stop tab5-osm-…` command displayed at the end. Volumes remain available. An interrupted PostgreSQL import cannot resume like PNG downloads: the program reports this and preserves its volumes; a new import requires a new instance name.

## Command-line mode

```powershell
py download_t5map.py "C:\Cartes\zone-topo.t5map" --center 48.39,-4.49 --radius-km 100 --min-zoom 0 --max-zoom 12
py download_t5map.py "C:\Cartes\zone-osm.t5map" --provider osm --center 48.39,-4.49 --radius-km 100 --min-zoom 0 --max-zoom 12 --estimate-only
py download_t5map.py "C:\Cartes\zone-osm.t5map" --provider osm --center 48.39,-4.49 --radius-km 100 --min-zoom 0 --max-zoom 12 --sd-root "E:\"
py download_t5map.py "C:\Cartes\zone-topo.t5map" --provider opentopomap --center 48.39,-4.49 --radius-km 100 --min-zoom 0 --max-zoom 12
```

Without `--provider`, command-line mode uses **OpenTopoMap**. OSM remains available with `--provider osm`.

Command-line search accepts `--city "Brest, France"`. If several results are possible, the program displays them and asks you to specify `--city-index`. It does not silently choose a city with the same name. `--url-template` also accepts a local or authorized XYZ server; `--header` supplies authentication supported by that server. The program contains no commercial key.

An old `z/x/y.png` directory or raster MBTiles file can be converted without downloading again:

```powershell
py build_t5map.py "C:\AncienCache" "C:\Cartes\ancien-osm.t5map" --cache-id osm
py build_t5map.py "C:\Topo.mbtiles" "C:\Cartes\ancien-topo.t5map" --cache-id opentopomap
```

Images must be 256 × 256 PNGs. Vector MBTiles is not raster data and must first be rendered on the PC.

## Handling interruptions

- **HTTP 403 or 429**: stop and wait; the server refused or limited requests. The message displays `Retry-After` if the server supplies it.
- **HTTPS certificate**: check the PC date and Python installation; certificate verification stays enabled.
- **Connection refused at 127.0.0.1**: OpenStreetMap is selected but its server is not running. Choose OpenTopoMap and leave its URL field blank to download directly.
- **Disk full or unwritable directory**: free some space or choose an accessible directory. The cache is preserved.

Error details identify the affected tile. Resuming the same area and directory reuses verified PNGs; an error does not publish a complete package. On the tablet, **Offline maps → Download an area** also uses OpenTopoMap on first launch, and displays the size and minimum duration before confirmation.

## Sources

- [Official OSM tile policy](https://operations.osmfoundation.org/policies/tiles/)
- [Local OSM rendering: Switch2OSM guide](https://switch2osm.org/serving-tiles/using-a-docker-container/)
- [Docker renderer and import commands](https://github.com/Overv/openstreetmap-tile-server)
- [OpenTopoMap usage, attribution and FAQ](https://opentopomap.org/about)

## Version 0.8.22: zoom and request rate

Zoom fields accept 0-22 (the level displayed on the tablet). Import downloads the levels actually available: up to 19 for OSM and 17 for OpenTopoMap; higher levels display enlarged parent tiles. The estimate shows both ranges. Existing packages remain compatible.

The **Requests per second** (`Requêtes par seconde`) field retains your choice between 0.1 and 100. The limit applies across all connections, and the public OpenTopoMap server remains limited to 0.5 requests/s. On the command line, add `--requests-per-second 0.5`. Pause allows the current request to finish and retains completed tiles. The [Maps guide](../../CARTES.md) also describes information mode and the tablet setting.
