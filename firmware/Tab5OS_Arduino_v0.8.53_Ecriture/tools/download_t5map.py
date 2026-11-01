#!/usr/bin/env python3
"""Télécharge une zone raster autorisée puis construit un paquet Tab5OS.

Le téléchargement est reprenable : chaque PNG valide déjà présent dans le
cache de travail est ignoré. Le script refuse volontairement le serveur public
tile.openstreetmap.org, dont la politique interdit le téléchargement hors ligne.
"""

from __future__ import annotations

import argparse
import json
import hashlib
import shutil
import ssl
import errno
from urllib.parse import urlencode
import tab5_map_geometry as geometry
from tab5_map_policy import native_range, request_rate
from concurrent.futures import FIRST_COMPLETED, ThreadPoolExecutor, wait
from dataclasses import dataclass
import math
import os
from pathlib import Path
import subprocess
import sys
import threading
import struct
import zlib
import time
from typing import Iterator
from urllib.error import HTTPError, URLError
from urllib.parse import urlparse
from urllib.request import Request, urlopen


PNG_MAGIC = b"\x89PNG\r\n\x1a\n"
MAX_TILE_BYTES = 512 * 1024
DEFAULT_AVERAGE_TILE_BYTES = 32 * 1024
PRESETS = {
    "world": (-180.0, -85.05112877, 180.0, 85.05112877),
}
PROVIDERS = {
    "osm": {"url": "http://127.0.0.1:8080/tile/{z}/{x}/{y}.png", "cache_id": "osm", "name": "OpenStreetMap", "max_zoom": 19},
    "opentopomap": {"url": "https://a.tile.opentopomap.org/{z}/{x}/{y}.png", "cache_id": "opentopomap", "name": "OpenTopoMap", "max_zoom": 17},
}


@dataclass(frozen=True)
class Tile:
    zoom: int
    x: int
    y: int


@dataclass(frozen=True)
class DownloadResult:
    tile: Tile
    state: str
    size: int = 0
    message: str = ""


class DownloadCancellation(threading.Event):
    def __init__(self, filename=None):
        super().__init__()
        self.filename = Path(filename) if filename else None

    def is_set(self):
        if self.filename is not None and self.filename.exists():
            self.set()
        return super().is_set()

    def wait(self, timeout=None):
        deadline = None if timeout is None else time.monotonic() + timeout
        while not self.is_set():
            remaining = 0.05 if deadline is None else deadline-time.monotonic()
            if remaining <= 0:
                return False
            super().wait(min(0.05,remaining))
        return True


class RateLimiter:
    def __init__(self, requests_per_second: float) -> None:
        self.interval = 1.0 / requests_per_second if requests_per_second else 0.0
        self.lock = threading.Lock()
        self.next_time = 0.0

    def wait(self, stopped=None) -> None:
        if not self.interval:
            return
        with self.lock:
            now = time.monotonic()
            delay = self.next_time - now
            if delay > 0:
                if stopped is not None:
                    if stopped.wait(delay):
                        raise SourceStopped("Source interrompue")
                else:
                    time.sleep(delay)
                now = time.monotonic()
            self.next_time = max(self.next_time, now) + self.interval


def parse_bbox(text: str) -> tuple[float, float, float, float]:
    try:
        values = tuple(float(part.strip()) for part in text.split(","))
    except ValueError as error:
        raise argparse.ArgumentTypeError("bbox attendu : ouest,sud,est,nord") from error
    if len(values) != 4:
        raise argparse.ArgumentTypeError("bbox attendu : ouest,sud,est,nord")
    west, south, east, north = values
    if not (-180 <= west < east <= 180 and
            -85.05112878 <= south < north <= 85.05112878):
        raise argparse.ArgumentTypeError("emprise géographique invalide")
    return values


def lon_to_x(longitude: float, zoom: int) -> int:
    count = 1 << zoom
    return max(0, min(count - 1, int((longitude + 180.0) / 360.0 * count)))


def lat_to_y(latitude: float, zoom: int) -> int:
    latitude = max(-85.05112878, min(85.05112878, latitude))
    radians = math.radians(latitude)
    value = (1.0 - math.asinh(math.tan(radians)) / math.pi) * 0.5
    count = 1 << zoom
    return max(0, min(count - 1, int(value * count)))


def tile_bounds(bbox: tuple[float, float, float, float], zoom: int
                ) -> tuple[int, int, int, int]:
    west, south, east, north = bbox
    return (lon_to_x(west, zoom), lon_to_x(east, zoom),
            lat_to_y(north, zoom), lat_to_y(south, zoom))


def count_tiles(bbox: tuple[float, float, float, float],
                min_zoom: int, max_zoom: int) -> int:
    total = 0
    for zoom in range(min_zoom, max_zoom + 1):
        x_min, x_max, y_min, y_max = tile_bounds(bbox, zoom)
        total += (x_max - x_min + 1) * (y_max - y_min + 1)
    return total


def iter_tiles(bbox: tuple[float, float, float, float],
               min_zoom: int, max_zoom: int) -> Iterator[Tile]:
    for zoom in range(min_zoom, max_zoom + 1):
        x_min, x_max, y_min, y_max = tile_bounds(bbox, zoom)
        for x in range(x_min, x_max + 1):
            for y in range(y_min, y_max + 1):
                yield Tile(zoom, x, y)


def tile_path(cache_dir: Path, tile: Tile) -> Path:
    return cache_dir / str(tile.zoom) / str(tile.x) / f"{tile.y}.png"


def valid_png_data(data: bytes) -> bool:
    if not 57 <= len(data) <= MAX_TILE_BYTES or not data.startswith(PNG_MAGIC):
        return False
    at = 8
    first = True
    image = False
    while len(data) - at >= 12:
        length = struct.unpack_from(">I", data, at)[0]
        if length > len(data) - at - 12:
            return False
        kind = data[at + 4:at + 8]
        block = memoryview(data)[at + 8:at + 8 + length]
        checksum = zlib.crc32(block, zlib.crc32(kind))
        if checksum != struct.unpack_from(">I", data, at + 8 + length)[0]:
            return False
        if first:
            if kind != b"IHDR" or length != 13 or struct.unpack_from(">II", block) != (256, 256):
                return False
            depth, color, compression, filtering, interlace = block[8:]
            valid_depth = ((color == 0 and depth in (1, 2, 4, 8, 16)) or
                           (color == 3 and depth in (1, 2, 4, 8)) or
                           (color in (2, 4, 6) and depth in (8, 16)))
            if not valid_depth or compression or filtering or interlace > 1:
                return False
            first = False
        elif kind == b"IHDR":
            return False
        if kind == b"IDAT" and length:
            image = True
        at += length + 12
        if kind == b"IEND":
            return length == 0 and image and at == len(data)
    return False


def valid_png(path: Path) -> bool:
    try:
        with path.open("rb") as source:
            return valid_png_data(source.read(MAX_TILE_BYTES + 1))
    except OSError:
        return False


def expanded_url(template: str, tile: Tile, subdomains: str) -> str:
    url = (template.replace("{z}", str(tile.zoom))
           .replace("{x}", str(tile.x))
           .replace("{y}", str(tile.y)))
    if "{s}" in url:
        if not subdomains:
            raise ValueError("l'URL utilise {s}, mais --subdomains est vide")
        url = url.replace("{s}", subdomains[(tile.x + tile.y) % len(subdomains)])
    return url


def parse_headers(values: list[str]) -> dict[str, str]:
    headers: dict[str, str] = {}
    for value in values:
        if ":" not in value:
            raise argparse.ArgumentTypeError(
                f"en-tête invalide {value!r}; format attendu Nom: valeur")
        name, content = value.split(":", 1)
        name = name.strip()
        content = content.strip()
        if not name or not content:
            raise argparse.ArgumentTypeError(f"en-tête invalide {value!r}")
        headers[name] = content
    return headers


def human_size(value: int) -> str:
    size = float(value)
    for unit in ("octets", "Kio", "Mio", "Gio", "Tio"):
        if size < 1024 or unit == "Tio":
            return f"{size:.1f} {unit}"
        size /= 1024
    return f"{size:.1f} Tio"


def human_duration(seconds: float) -> str:
    seconds = max(1, math.ceil(seconds))
    hours, remainder = divmod(seconds, 3600)
    minutes, seconds = divmod(remainder, 60)
    if hours:
        return f"{hours} h {minutes:02d} min"
    if minutes:
        return f"{minutes} min {seconds:02d} s"
    return f"{seconds} s"


def network_error(error: Exception) -> str:
    reason = error.reason if isinstance(error, URLError) else error
    if isinstance(reason, ssl.SSLCertVerificationError):
        return "Certificat HTTPS non vérifié : vérifier la date du PC, les certificats Windows et l’installation Python. " + str(reason)
    if isinstance(reason, ConnectionRefusedError):
        return "Le serveur refuse la connexion. Pour OpenTopoMap, laisser l’URL vide ; pour OSM local, démarrer le serveur préparé sur ce PC."
    if isinstance(reason, (TimeoutError, ConnectionError)):
        return "Le serveur n’a pas répondu : " + str(reason)
    return str(reason)


def download_one(tile: Tile, *, cache_dir: Path, template: str,
                 subdomains: str, headers: dict[str, str], timeout: float,
                 retries: int, limiter: RateLimiter, stopped=None) -> DownloadResult:
    if stopped is not None and stopped.is_set():
        raise SourceStopped("Source interrompue")
    destination = tile_path(cache_dir, tile)
    if valid_png(destination):
        return DownloadResult(tile, "cached", destination.stat().st_size)
    temporary = destination.with_suffix(".png.part")
    try:
        if temporary.exists():
            temporary.unlink()
    except OSError:
        pass

    url = expanded_url(template, tile, subdomains)
    for attempt in range(retries + 1):
        limiter.wait(stopped)
        if stopped is not None and stopped.is_set():
            raise SourceStopped("Source interrompue")
        try:
            request = Request(url, headers=headers, method="GET")
            with urlopen(request, timeout=timeout) as response:
                data = response.read(MAX_TILE_BYTES + 1)
            if len(data) > MAX_TILE_BYTES:
                return DownloadResult(
                    tile, "failed", message="PNG supérieur à 512 Kio")
            if not valid_png_data(data):
                if stopped is not None:
                    stopped.set()
                raise SourceStopped("La source ne renvoie pas un PNG 256 × 256 valide à z%d/%d/%d. Vérifier l’URL et le serveur." % (tile.zoom, tile.x, tile.y))
            destination.parent.mkdir(parents=True, exist_ok=True)
            with temporary.open("wb") as output:
                output.write(data)
                output.flush()
                os.fsync(output.fileno())
            os.replace(temporary, destination)
            return DownloadResult(tile, "downloaded", len(data))
        except HTTPError as error:
            if error.code in (401, 403, 429):
                if stopped is not None:
                    stopped.set()
                raise SourceStopped(f"HTTP {error.code} à z{tile.zoom}/{tile.x}/{tile.y}: arrêt de la source; réessayer plus tard. Retry-After: {error.headers.get('Retry-After', 'non fourni')}")
            if error.code in (404, 410):
                return DownloadResult(tile, "missing", message=f"HTTP {error.code}")
            retry_after = error.headers.get("Retry-After") if error.headers else None
            if attempt >= retries:
                return DownloadResult(tile, "failed", message=f"HTTP {error.code}")
            try:
                delay = float(retry_after) if retry_after else 2 ** attempt
            except ValueError:
                delay = 2 ** attempt
            if stopped is not None:
                if stopped.wait(min(max(delay, 0.5), 60.0)):
                    raise SourceStopped("Source interrompue")
            else:
                time.sleep(min(max(delay, 0.5), 60.0))
        except (URLError, TimeoutError, OSError) as error:
            reason = error.reason if isinstance(error, URLError) else error
            if isinstance(reason, ssl.SSLCertVerificationError):
                if stopped is not None:
                    stopped.set()
                raise SourceStopped("Échec HTTPS à z%d/%d/%d : " % (tile.zoom, tile.x, tile.y) + network_error(error))
            if isinstance(error, OSError) and error.errno in (errno.ENOSPC, errno.EACCES, errno.EROFS):
                if stopped is not None:
                    stopped.set()
                raise SourceStopped("Écriture du cache impossible : " + str(error))
            if attempt >= retries:
                if stopped is not None:
                    stopped.set()
                raise SourceStopped("Source inaccessible à z%d/%d/%d après plusieurs essais : " % (tile.zoom, tile.x, tile.y) + network_error(error))
            if stopped is not None:
                if stopped.wait(min(2 ** attempt, 10)):
                    raise SourceStopped("Source interrompue")
            else:
                time.sleep(min(2 ** attempt, 10))
    return DownloadResult(tile, "failed", message="échec inconnu")


def bounded_download(tiles: Iterator[Tile], total: int, workers: int,
                     worker_kwargs: dict[str, object], error_file: Path, cancel_file=None
                     ) -> tuple[int, int, int, int, int]:
    completed = downloaded = cached = missing = failed = bytes_total = 0
    started = time.monotonic()
    last_report = started
    errors: list[str] = []
    iterator = iter(tiles)
    worker_kwargs = dict(worker_kwargs, stopped=DownloadCancellation(cancel_file))
    with ThreadPoolExecutor(max_workers=workers,
                            thread_name_prefix="t5map") as executor:
        pending = {}
        for _ in range(workers * 3):
            try:
                tile = next(iterator)
            except StopIteration:
                break
            pending[executor.submit(download_one, tile, **worker_kwargs)] = tile

        while pending:
            done, _ = wait(pending, return_when=FIRST_COMPLETED)
            for future in done:
                tile = pending.pop(future)
                try:
                    result = future.result()
                except SourceStopped:
                    for queued in pending:
                        queued.cancel()
                    raise
                except Exception as error:
                    result = DownloadResult(tile, "failed", message=str(error))
                completed += 1
                if result.message:
                    print(f"z{tile.zoom}/{tile.x}/{tile.y} : {result.message}", flush=True)
                bytes_total += result.size
                if result.state == "downloaded":
                    downloaded += 1
                elif result.state == "cached":
                    cached += 1
                elif result.state == "missing":
                    missing += 1
                    errors.append(
                        f"z{tile.zoom}/{tile.x}/{tile.y}: {result.message}")
                else:
                    failed += 1
                    errors.append(
                        f"z{tile.zoom}/{tile.x}/{tile.y}: {result.message}")

                if completed == total or completed % 100 == 0 or time.monotonic() - last_report >= 1.0:
                    last_report = time.monotonic()
                    elapsed = max(time.monotonic() - started, 0.001)
                    rate = completed / elapsed
                    remaining = (total - completed) / rate if rate else 0
                    print(f"{completed:,}/{total:,} | nouveaux={downloaded:,} "
                          f"déjà présents={cached:,} | erreurs={missing + failed:,} "
                          f"| {rate:.1f} t/s | reste ~{remaining / 60:.1f} min",
                          flush=True)
                try:
                    next_tile = next(iterator)
                except StopIteration:
                    continue
                pending[executor.submit(
                    download_one, next_tile, **worker_kwargs)] = next_tile

    if errors:
        error_file.parent.mkdir(parents=True, exist_ok=True)
        error_file.write_text("\n".join(errors) + "\n", encoding="utf-8")
    elif error_file.exists():
        error_file.unlink()
    return downloaded, cached, missing, failed, bytes_total


def validate_template(template: str, subdomains: str) -> None:
    for marker in ("{z}", "{x}", "{y}"):
        if marker not in template:
            raise ValueError(f"l'URL ne contient pas {marker}")
    sample = (template.replace("{z}", "0").replace("{x}", "0")
              .replace("{y}", "0"))
    if "{s}" in sample:
        if not subdomains:
            raise ValueError("l'URL utilise {s}; fournir --subdomains")
        sample = sample.replace("{s}", subdomains[0])
    parsed = urlparse(sample)
    if parsed.scheme not in {"http", "https"} or not parsed.hostname:
        raise ValueError("l'URL doit commencer par http:// ou https://")
    hostname = parsed.hostname.lower().rstrip(".")
    if hostname == "tile.openstreetmap.org" or hostname.endswith(".tile.openstreetmap.org"):
        raise ValueError(
            "tile.openstreetmap.org interdit le préchargement hors ligne; "
            "utiliser un fournisseur qui l'autorise ou un serveur local")


class SourceStopped(RuntimeError):
    pass


def search_city(query, cache_dir=None):
    cache_dir = Path(cache_dir or Path.home() / ".tab5maps" / "cities")
    cache_dir.mkdir(parents=True, exist_ok=True)
    path = cache_dir / (hashlib.sha256(query.strip().lower().encode()).hexdigest()[:24] + ".json")
    if path.exists():
        return json.loads(path.read_text(encoding="utf-8"))
    # Explicit searches only, cached and at most one per second across processes.
    marker = cache_dir / "last-search"
    if marker.exists():
        time.sleep(max(0, 1.1 - (time.time() - marker.stat().st_mtime)))
    marker.touch()
    url = "https://nominatim.openstreetmap.org/search?" + urlencode({"q": query, "format": "jsonv2", "limit": 8})
    with urlopen(Request(url, headers={"User-Agent": "Tab5OS-MapBuilder/0.8.1", "Accept": "application/json"}), timeout=30) as response:
        data = json.loads(response.read(128 * 1024))
    result = [{"name": str(v["display_name"]), "lat": float(v["lat"]), "lon": float(v["lon"])} for v in data[:8]]
    path.write_text(json.dumps(result, ensure_ascii=False), encoding="utf-8")
    return result


def resolve_region(args):
    if args.city:
        matches = search_city(args.city)
        if not matches:
            raise ValueError("Aucune ville trouvée")
        if len(matches) > 1 and args.city_index is None:
            for index, match in enumerate(matches):
                print(f"{index}: {match['name']} ({match['lat']}, {match['lon']})")
            raise ValueError("Préciser --city-index pour choisir le résultat, ou utiliser --center latitude,longitude")
        center = matches[args.city_index or 0]
        print("Centre : " + center["name"])
        lat, lon = center["lat"], center["lon"]
    else:
        lat, lon = (float(v) for v in args.center.split(","))
    if not -geometry.MAX_LAT <= lat <= geometry.MAX_LAT or not -180 <= lon <= 180 or not 0.1 <= args.radius_km <= 5000:
        raise ValueError("Coordonnées invalides ou rayon hors de 0,1–5000 km")
    return {"lat": lat, "lon": ((lon+180)%360)-180, "radius": args.radius_km, "min_zoom": args.min_zoom, "max_zoom": args.max_zoom}


def ensure_job(cache_dir, metadata):
    path = cache_dir / "tab5-job.json"
    if path.exists():
        previous = json.loads(path.read_text(encoding="utf-8"))
        if previous != metadata:
            raise ValueError("Ce cache appartient à une autre source ou zone. Choisir un autre dossier de sortie/cache pour éviter de mélanger les cartes.")
    elif any(cache_dir.iterdir()):
        raise ValueError("Cache ancien sans provenance. Importer ses PNG avec build_t5map.py, ou choisir un nouveau cache.")
    else:
        temporary = path.with_suffix(".json.part")
        temporary.write_text(json.dumps(metadata, indent=2, ensure_ascii=False), encoding="utf-8")
        os.replace(temporary, path)


def validate_sd_capacity(destination, files, previous=None, source_manifest=None, announce=True):
    import build_t5map as builder
    existing = {path.name.casefold(): path for path in destination.iterdir()
                if path.is_file() and path.suffix.lower() == ".t5map"}
    current = {path.name.casefold() for path in files}
    remaining = set(existing) | current
    if len(remaining) > builder.MAX_OFFLINE_PACKS and previous and source_manifest and previous.get("cache_id") == source_manifest.get("cache_id"):
        # Only old, unchanged volumes owned by this export will be removed by
        # commit_manifest. Personal, legacy and modified files still count.
        for record in previous["volumes"]:
            key = record["name"].casefold()
            old = existing.get(key)
            if key in current or old is None or old.is_symlink():
                continue
            try:
                if old.stat().st_size == record["size"] and builder.file_digest(old) == record["sha256"]:
                    remaining.discard(key)
            except OSError:
                continue
            if len(remaining) <= builder.MAX_OFFLINE_PACKS:
                break
    if len(remaining) > builder.MAX_OFFLINE_PACKS:
        raise ValueError(f"La SD contiendrait {len(remaining)} paquets .t5map ; Tab5OS en lit au maximum 128, toutes cartes confondues. Déplacer les zones inutilisées puis recommencer la copie.")
    if announce:
        print(f"SD : au plus {len(remaining)} / 128 paquets après copie.", flush=True)


def copy_to_sd(files, sd_root, base=None):
    import build_t5map as builder
    destination = Path(sd_root) / "System" / "maps" / "offline"
    destination.mkdir(parents=True, exist_ok=True)
    source_manifest = builder.read_manifest(base) if base is not None else None
    target_base = destination / base.name if base is not None else None
    previous = builder.read_manifest(target_base) if target_base is not None else None
    expected = {record["name"]: record for record in source_manifest["volumes"]} if source_manifest else {}
    validate_sd_capacity(destination, files, previous, source_manifest)
    records = []
    for source in files:
        target = destination / source.name
        if source.resolve() == target.resolve():
            records.append({"name": target.name, "size": target.stat().st_size, "sha256": builder.file_digest(target)})
            continue
        if shutil.disk_usage(destination).free < source.stat().st_size:
            raise ValueError("Espace libre insuffisant sur la carte SD")
        temporary = target.with_suffix(".t5map.part")
        digest = hashlib.sha256()
        with source.open("rb") as reader, temporary.open("wb") as writer:
            for block in iter(lambda: reader.read(1024 * 1024), b""):
                writer.write(block); digest.update(block)
            writer.flush(); os.fsync(writer.fileno())
        if temporary.stat().st_size != source.stat().st_size:
            raise ValueError("Copie SD incomplète")
        record = {"name": target.name, "size": temporary.stat().st_size, "sha256": digest.hexdigest()}
        if source_manifest and expected.get(target.name) != record:
            raise ValueError("Le paquet source a changé depuis sa préparation. Relancer la construction.")
        os.replace(temporary, target)
        records.append(record)
        print("SD : " + str(target), flush=True)
    if source_manifest:
        builder.commit_manifest(target_base, [destination / source.name for source in files],
                                source_manifest["cache_id"], previous, records)
    validate_sd_capacity(destination, [], announce=False)


def main() -> int:
    parser = argparse.ArgumentParser(description="Cartes OpenStreetMap / OpenTopoMap pour Tab5OS, avec reprise et paquets FAT32.")
    parser.add_argument("destination", type=Path, help="fichier .t5map final")
    zone = parser.add_mutually_exclusive_group(required=True)
    zone.add_argument("--preset", choices=sorted(PRESETS))
    zone.add_argument("--bbox", type=parse_bbox, help="ouest,sud,est,nord")
    zone.add_argument("--center", help="latitude,longitude")
    zone.add_argument("--city", help="recherche de ville explicite, sans autocomplétion")
    parser.add_argument("--city-index", type=int)
    parser.add_argument("--radius-km", type=float, default=100)
    parser.add_argument("--provider", choices=sorted(PROVIDERS), default="opentopomap", help="opentopomap par défaut ; osm utilise votre serveur local")
    parser.add_argument("--url-template", help="serveur local ou source autorisée pour le style choisi")
    parser.add_argument("--subdomains", default="")
    parser.add_argument("--min-zoom", type=int, default=0)
    parser.add_argument("--max-zoom", type=int, default=12, help="zoom d'affichage 0–22 ; au-delà de la source, importe les tuiles parentes")
    parser.add_argument("--cache-dir", type=Path)
    parser.add_argument("--name")
    parser.add_argument("--user-agent", default="Tab5OS-MapBuilder/0.8.1")
    parser.add_argument("--header", action="append", default=[])
    parser.add_argument("--workers", type=int, default=2)
    parser.add_argument("--requests-per-second", type=float, default=4.0, help="fréquence totale 0,1–100 requêtes/s ; OpenTopoMap public plafonné à 0,5")
    parser.add_argument("--timeout", type=float, default=60.0)
    parser.add_argument("--retries", type=int, default=3)
    parser.add_argument("--max-tiles", type=int, default=250_000)
    parser.add_argument("--max-volume-mib", type=int, default=1900)
    parser.add_argument("--sd-root", type=Path)
    parser.add_argument("--cancel-file", type=Path, help="arrêter après la requête en cours lorsque ce fichier existe")
    parser.add_argument("--estimate-only", action="store_true")
    parser.add_argument("--yes", action="store_true")
    args = parser.parse_args()
    provider = PROVIDERS[args.provider]
    view_low, view_high = args.min_zoom, args.max_zoom
    try:
        args.min_zoom, args.max_zoom = native_range(view_low, view_high, provider["max_zoom"])
    except ValueError as error:
        parser.error(str(error))
    if not 1 <= args.workers <= 8 or not 0.1 <= args.requests_per_second <= 100 or not 0 <= args.retries <= 10:
        parser.error("Paramètres réseau invalides")
    if not 64 <= args.max_volume_mib <= 2000 or args.max_tiles <= 0 or not 1 <= args.timeout <= 600:
        parser.error("Limites invalides : volume de 64 à 2000 Mio, nombre de tuiles positif, délai de 1 à 600 s")
    template = args.url_template or provider["url"]
    try:
        validate_template(template, args.subdomains)
        headers = parse_headers(args.header)
        if args.city or args.center:
            region = resolve_region(args)
            total, levels = geometry.estimate(region)
            iterator = (Tile(z,x,y) for z,x,y in geometry.tiles(region))
            zone_identity = region
            print(f"Rayon : {args.radius_km:g} km")
        else:
            bbox = PRESETS[args.preset] if args.preset else args.bbox
            total = count_tiles(bbox, args.min_zoom, args.max_zoom)
            iterator = iter_tiles(bbox, args.min_zoom, args.max_zoom)
            zone_identity = {"bbox": list(bbox), "min_zoom": args.min_zoom, "max_zoom": args.max_zoom}
    except (ValueError, IndexError, OSError) as error:
        parser.error(str(error))
    headers.setdefault("User-Agent", args.user_agent)
    headers.setdefault("Accept", "image/png")
    host = urlparse(template).hostname.lower().rstrip(".")
    if host == "tile.opentopomap.org" or host.endswith(".tile.opentopomap.org"):
        args.workers = 1
        args.requests_per_second = request_rate(args.requests_per_second, True)
        print("OpenTopoMap public : 1 connexion, au plus 1 tuile/2 s ; arrêt sur HTTP 403/429.")
        print("Durée minimale hors cache : " + human_duration(total / args.requests_per_second) + ". Le temps réseau s’ajoute à cette estimation.")
    print(f"Affichage demandé : z{view_low}–{view_high} ; source téléchargée : z{args.min_zoom}–{args.max_zoom}.", flush=True)
    print(f"{provider['name']} · z{args.min_zoom}–{args.max_zoom} · {total:,} tuiles", flush=True)
    print(f"Fréquence maximale : {args.requests_per_second:g} requêtes/s · connexions : {args.workers}", flush=True)
    print("Estimation à 32 Kio/tuile : " + human_size(total * DEFAULT_AVERAGE_TILE_BYTES), flush=True)
    print("Espace PC indicatif (cache + paquets) : " + human_size(total * DEFAULT_AVERAGE_TILE_BYTES * 2) + "; taille réelle variable.", flush=True)
    estimated_volumes = max(1, math.ceil(total * (DEFAULT_AVERAGE_TILE_BYTES + 17) / (args.max_volume_mib * 1024 * 1024)))
    print(f"Paquets indicatifs : ~{estimated_volumes}, limite par fichier {args.max_volume_mib} Mio ; au maximum 128 paquets sur la SD.", flush=True)
    if total > args.max_tiles:
        parser.error(f"{total:,} tuiles > --max-tiles {args.max_tiles:,}. Réduire le zoom ou augmenter cette limite après estimation.")
    if args.estimate_only:
        return 0
    if not args.yes and input("Télécharger/reprendre ? [o/N] ").strip().lower() not in {"o", "oui", "y", "yes"}:
        return 1
    destination = args.destination.expanduser().resolve().with_suffix(".t5map")
    cache_dir = args.cache_dir.expanduser().resolve() if args.cache_dir else destination.parent / (destination.stem + "_cache_xyz")
    cache_dir.mkdir(parents=True, exist_ok=True)
    try:
        ensure_job(cache_dir, {"version": 1, "provider": args.provider, "source_sha256": hashlib.sha256(template.encode()).hexdigest(), "zone": zone_identity})
    except ValueError as error:
        parser.error(str(error))
    worker_kwargs = {"cache_dir": cache_dir, "template": template, "subdomains": args.subdomains, "headers": headers, "timeout": args.timeout, "retries": args.retries, "limiter": RateLimiter(args.requests_per_second)}
    print("Cache reprenable : " + str(cache_dir), flush=True)
    print("Vérification du cache et connexion à " + host + "…", flush=True)
    try:
        downloaded, cached, missing, failed, size = bounded_download(iterator, total, args.workers, worker_kwargs, cache_dir / "download_errors.txt", args.cancel_file)
    except (KeyboardInterrupt, SourceStopped) as error:
        print("Téléchargement interrompu. Relancer conserve les tuiles terminées. " + str(error), flush=True)
        return 130
    if missing or failed:
        print(f"Zone incomplète : {missing} absentes, {failed} erreurs. Relancer pour compléter ; aucun paquet complet annoncé.", flush=True)
        return 2
    import build_t5map as builder
    tiles = builder.make_iterator(cache_dir, args.min_zoom, args.max_zoom, None)
    plans = builder.plan_volumes(tiles, args.max_volume_mib * 1024 * 1024)
    if sum(p.count for p in plans) != total:
        raise ValueError("Le cache ne correspond pas à la zone demandée")
    print("Construction des paquets…", flush=True)
    subprocess.run([sys.executable, str(Path(__file__).with_name("build_t5map.py")), str(cache_dir), str(destination), "--cache-id", provider["cache_id"], "--name", args.name or provider["name"], "--min-zoom", str(args.min_zoom), "--max-zoom", str(args.max_zoom), "--max-volume-mib", str(args.max_volume_mib)], check=True)
    files = [builder.output_path(destination, index, len(plans)) for index in range(len(plans))]
    if args.sd_root:
        copy_to_sd(files, args.sd_root, destination)
    print("Terminé : " + ", ".join(str(v) for v in files), flush=True)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print("Échec : " + str(error) + "\nLes tuiles terminées restent dans le cache pour la reprise.", file=sys.stderr)
        raise SystemExit(2)
