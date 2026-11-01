#!/usr/bin/env python3
"""Construit des paquets raster T5MAP depuis un MBTiles ou un cache XYZ.

Le programme ne télécharge rien. Il convertit des tuiles PNG déjà obtenues
auprès d'une source autorisant l'usage hors ligne. Les volumes restent sous la
limite de lecture de 2 Gio du firmware et sont lus directement par Tab5OS sans SQLite sur l'ESP32-P4.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import re
import shutil
import sqlite3
import struct
import tempfile
from typing import Callable, Iterator, NamedTuple


MAGIC = b"T5MAP01\0"
VERSION = 1
HEADER_SIZE = 128
ENTRIES_PER_BLOCK = 1024
# Arduino File::seek calls 32-bit signed fseek in the pinned ESP32 toolchain.
MAX_READER_BYTES = (1 << 31) - 1
DEFAULT_VOLUME_MIB = 1900
MAX_VOLUME_MIB = 2000
MAX_OFFLINE_PACKS = 128
PNG_MAGIC = b"\x89PNG\r\n\x1a\n"
HEADER = struct.Struct("<8s8I2Q24s40s8s")
ENTRY = struct.Struct("<QII")
DIRECTORY = struct.Struct("<QII")
SAFE_ID = re.compile(r"^[A-Za-z0-9_-]{1,23}$")


class TileInfo(NamedTuple):
    zoom: int
    x: int
    y: int
    size: int
    data: bytes | None


class VolumePlan(NamedTuple):
    count: int
    data_bytes: int
    min_key: int
    max_key: int


def tile_key(zoom: int, x: int, y: int) -> int:
    return (zoom << 58) | (x << 29) | y


def xyz_y_from_tms(zoom: int, row: int) -> int:
    return (1 << zoom) - 1 - row


def lon_to_x(longitude: float, zoom: int) -> int:
    count = 1 << zoom
    return max(0, min(count - 1, int((longitude + 180.0) / 360.0 * count)))


def lat_to_y(latitude: float, zoom: int) -> int:
    latitude = max(-85.05112878, min(85.05112878, latitude))
    radians = math.radians(latitude)
    value = (1.0 - math.asinh(math.tan(radians)) / math.pi) * 0.5
    count = 1 << zoom
    return max(0, min(count - 1, int(value * count)))


def in_bbox(zoom: int, x: int, y: int,
            bbox: tuple[float, float, float, float] | None) -> bool:
    if bbox is None:
        return True
    west, south, east, north = bbox
    if west > east:
        raise ValueError("les emprises traversant l'antiméridien ne sont pas prises en charge")
    return (lon_to_x(west, zoom) <= x <= lon_to_x(east, zoom) and
            lat_to_y(north, zoom) <= y <= lat_to_y(south, zoom))


def mbtiles_iterator(path: Path, min_zoom: int, max_zoom: int,
                     bbox: tuple[float, float, float, float] | None,
                     with_data: bool) -> Iterator[TileInfo]:
    connection = sqlite3.connect(f"file:{path.as_posix()}?mode=ro", uri=True)
    try:
        payload = "tile_data" if with_data else "NULL"
        query = f"""
            SELECT zoom_level, tile_column, tile_row,
                   length(tile_data), substr(tile_data, 1, 8), {payload}
            FROM tiles
            WHERE zoom_level BETWEEN ? AND ?
            ORDER BY zoom_level ASC, tile_column ASC, tile_row DESC
        """
        for zoom, x, tms_y, size, signature, data in connection.execute(
                query, (min_zoom, max_zoom)):
            y = xyz_y_from_tms(zoom, tms_y)
            if signature != PNG_MAGIC or not in_bbox(zoom, x, y, bbox):
                continue
            yield TileInfo(zoom, x, y, size, bytes(data) if data is not None else None)
    finally:
        connection.close()


def numeric_directories(path: Path) -> list[Path]:
    return sorted((item for item in path.iterdir()
                   if item.is_dir() and item.name.isdigit()),
                  key=lambda item: int(item.name))


def xyz_iterator(root: Path, min_zoom: int, max_zoom: int,
                 bbox: tuple[float, float, float, float] | None,
                 with_data: bool) -> Iterator[TileInfo]:
    for zoom_dir in numeric_directories(root):
        zoom = int(zoom_dir.name)
        if zoom < min_zoom or zoom > max_zoom:
            continue
        for x_dir in numeric_directories(zoom_dir):
            x = int(x_dir.name)
            files = sorted((item for item in x_dir.iterdir()
                            if item.is_file() and item.stem.isdigit() and
                            item.suffix.lower() == ".png"),
                           key=lambda item: int(item.stem))
            for tile_path in files:
                y = int(tile_path.stem)
                if not in_bbox(zoom, x, y, bbox):
                    continue
                with tile_path.open("rb") as source:
                    if source.read(8) != PNG_MAGIC:
                        continue
                    data = tile_path.read_bytes() if with_data else None
                yield TileInfo(zoom, x, y, tile_path.stat().st_size, data)


def make_iterator(source: Path, min_zoom: int, max_zoom: int,
                  bbox: tuple[float, float, float, float] | None
                  ) -> Callable[[bool], Iterator[TileInfo]]:
    if source.is_dir():
        return lambda with_data: xyz_iterator(
            source, min_zoom, max_zoom, bbox, with_data)
    if source.suffix.lower() in {".mbtiles", ".sqlite", ".sqlite3"}:
        return lambda with_data: mbtiles_iterator(
            source, min_zoom, max_zoom, bbox, with_data)
    raise ValueError("la source doit être un fichier MBTiles ou un dossier z/x/y.png")


def projected_size(count: int, data_bytes: int) -> int:
    directories = (count + ENTRIES_PER_BLOCK - 1) // ENTRIES_PER_BLOCK
    return HEADER_SIZE + directories * DIRECTORY.size + count * ENTRY.size + data_bytes


def plan_volumes(iterator: Callable[[bool], Iterator[TileInfo]],
                 max_volume_bytes: int) -> list[VolumePlan]:
    if not HEADER_SIZE < max_volume_bytes <= MAX_READER_BYTES:
        raise ValueError("le volume doit rester sous 2 Gio pour être lu entièrement par Tab5OS")
    plans: list[VolumePlan] = []
    count = data_bytes = 0
    first_key = last_key = -1
    previous_key = -1
    for tile in iterator(False):
        key = tile_key(tile.zoom, tile.x, tile.y)
        if key <= previous_key:
            raise ValueError("les tuiles ne sont pas strictement triées ou contiennent un doublon")
        previous_key = key
        if count and projected_size(count + 1, data_bytes + tile.size) > max_volume_bytes:
            plans.append(VolumePlan(count, data_bytes, first_key, last_key))
            if len(plans) >= MAX_OFFLINE_PACKS:
                raise ValueError("La zone dépasserait 128 paquets, limite totale lue sur la SD. Réduire le rayon ou le zoom.")
            count = data_bytes = 0
            first_key = -1
        if projected_size(1, tile.size) > max_volume_bytes:
            raise ValueError(f"tuile trop grande à z{tile.zoom}/{tile.x}/{tile.y}")
        if first_key < 0:
            first_key = key
        last_key = key
        count += 1
        data_bytes += tile.size
    if count:
        plans.append(VolumePlan(count, data_bytes, first_key, last_key))
    if not plans:
        raise ValueError("aucune tuile PNG ne correspond aux filtres")
    return plans


def fixed_text(value: str, size: int) -> bytes:
    data = value.encode("utf-8")[:size - 1]
    data = data.decode("utf-8", errors="ignore").encode("utf-8")
    return data + bytes(size - len(data))


class VolumeWriter:
    def __init__(self, path: Path, plan: VolumePlan, cache_id: str,
                 name: str) -> None:
        if projected_size(plan.count, plan.data_bytes) > MAX_READER_BYTES:
            raise ValueError("le volume dépasserait la limite de lecture de 2 Gio de Tab5OS")
        self.path = path
        self.plan = plan
        self.cache_id = cache_id
        self.name = name
        self.count = 0
        self.previous_key = -1
        self.directory: list[tuple[int, int, int]] = []
        self.directory_count = (
            plan.count + ENTRIES_PER_BLOCK - 1) // ENTRIES_PER_BLOCK
        self.directory_offset = HEADER_SIZE
        self.index_offset = self.directory_offset + self.directory_count * DIRECTORY.size
        self.data_offset = self.index_offset + plan.count * ENTRY.size
        self.path.parent.mkdir(parents=True, exist_ok=True)
        self.temporary = self.path.with_suffix(".t5map.part")
        self.output = self.temporary.open("w+b")
        self.output.seek(self.data_offset)
        self.index = tempfile.TemporaryFile()

    def add(self, tile: TileInfo) -> None:
        if tile.data is None or len(tile.data) != tile.size:
            raise ValueError("données de tuile absentes ou tronquées")
        key = tile_key(tile.zoom, tile.x, tile.y)
        if key <= self.previous_key:
            raise ValueError("ordre de clés incorrect pendant l'écriture")
        offset = self.output.tell()
        if offset + tile.size > MAX_READER_BYTES:
            raise ValueError("le volume dépasserait la limite de lecture de 2 Gio de Tab5OS")
        if self.count % ENTRIES_PER_BLOCK == 0:
            remaining = self.plan.count - self.count
            self.directory.append((key, self.count,
                                   min(ENTRIES_PER_BLOCK, remaining)))
        self.output.write(tile.data)
        self.index.write(ENTRY.pack(key, offset, tile.size))
        self.previous_key = key
        self.count += 1

    def finish(self, publish: bool = True) -> None:
        if self.count != self.plan.count or len(self.directory) != self.directory_count:
            raise ValueError("le nombre de tuiles écrit ne correspond pas au plan")
        self.output.seek(self.directory_offset)
        for item in self.directory:
            self.output.write(DIRECTORY.pack(*item))
        self.output.seek(self.index_offset)
        self.index.seek(0)
        shutil.copyfileobj(self.index, self.output, 1024 * 1024)
        self.output.seek(0)
        self.output.write(HEADER.pack(
            MAGIC, VERSION, HEADER_SIZE, self.plan.count,
            self.directory_count, ENTRIES_PER_BLOCK,
            self.directory_offset, self.index_offset, self.data_offset,
            self.plan.min_key, self.plan.max_key,
            fixed_text(self.cache_id, 24), fixed_text(self.name, 40), bytes(8)))
        self.output.flush()
        os.fsync(self.output.fileno())
        self.index.close()
        self.output.close()
        if publish:
            os.replace(self.temporary, self.path)


def output_path(base: Path, volume_index: int, volume_count: int) -> Path:
    if volume_count == 1:
        return base if base.suffix.lower() == ".t5map" else base.with_suffix(".t5map")
    stem = base.with_suffix("") if base.suffix else base
    return stem.parent / f"{stem.name}_{volume_index + 1:02d}.t5map"


def manifest_path(base: Path) -> Path:
    return base.with_suffix(".t5map.manifest.json")


def file_digest(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def read_manifest(base: Path) -> dict | None:
    """Only recognize this exporter and file names belonging to this exact base."""
    path = manifest_path(base)
    if not path.exists():
        return None
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
        pattern = re.compile(re.escape(base.with_suffix("").name) + r"(?:_[0-9]{2,})?\.t5map", re.IGNORECASE)
        if (data.get("version") != 1 or data.get("base") != base.with_suffix(".t5map").name
                or not isinstance(data.get("volumes"), list)):
            raise ValueError("format")
        names = set()
        for record in data["volumes"]:
            name = record["name"]
            if (not pattern.fullmatch(name) or name in names
                    or not isinstance(record["size"], int) or record["size"] < HEADER_SIZE
                    or not re.fullmatch(r"[0-9a-f]{64}", record["sha256"])):
                raise ValueError("volume")
            names.add(name)
        return data
    except (OSError, ValueError, KeyError, TypeError, AttributeError):
        print("Ancien manifeste illisible : ses fichiers sont conservés.", flush=True)
        return None


def commit_manifest(base: Path, files: list[Path], cache_id: str,
                    previous: dict | None, records: list[dict] | None = None) -> None:
    records = records if records is not None else [
        {"name": path.name, "size": path.stat().st_size, "sha256": file_digest(path)} for path in files]
    data = {"version": 1, "base": base.with_suffix(".t5map").name,
            "cache_id": cache_id, "volumes": records}
    path = manifest_path(base)
    temporary = path.with_suffix(".json.part")
    with temporary.open("w", encoding="utf-8") as output:
        json.dump(data, output, ensure_ascii=False, indent=2)
        output.flush(); os.fsync(output.fileno())
    os.replace(temporary, path)
    if previous is None or previous.get("cache_id") != cache_id:
        return
    current = {record["name"] for record in records}
    for record in previous["volumes"]:
        if record["name"] in current:
            continue
        old = base.parent / record["name"]
        try:
            if (not old.is_symlink() and old.is_file() and old.stat().st_size == record["size"]
                    and file_digest(old) == record["sha256"]):
                old.unlink()
                print("Ancien volume remplacé : " + str(old), flush=True)
            elif old.exists():
                print("Ancien volume modifié, conservé : " + str(old), flush=True)
        except OSError as error:
            print("Ancien volume conservé : " + str(old) + " (" + str(error) + ")", flush=True)


def parse_bbox(text: str | None) -> tuple[float, float, float, float] | None:
    if not text:
        return None
    values = tuple(float(part.strip()) for part in text.split(","))
    if len(values) != 4:
        raise argparse.ArgumentTypeError("bbox attendu : ouest,sud,est,nord")
    west, south, east, north = values
    if not (-180 <= west <= 180 and -180 <= east <= 180 and
            -85.05112878 <= south < north <= 85.05112878):
        raise argparse.ArgumentTypeError("emprise géographique invalide")
    return values


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path,
                        help="fichier MBTiles ou dossier de cache z/x/y.png")
    parser.add_argument("destination", type=Path,
                        help="fichier .t5map ou préfixe des volumes")
    parser.add_argument("--cache-id", default="osm",
                        help="cache_id du style Tab5OS (défaut : osm)")
    parser.add_argument("--name", default="Carte hors ligne",
                        help="nom affiché dans les journaux")
    parser.add_argument("--min-zoom", type=int, default=0)
    parser.add_argument("--max-zoom", type=int, default=20)
    parser.add_argument("--bbox", type=parse_bbox,
                        help="ouest,sud,est,nord en degrés")
    parser.add_argument("--max-volume-mib", type=int, default=DEFAULT_VOLUME_MIB,
                        help="taille maximale par volume : 1900 Mio par défaut, 2000 au maximum")
    args = parser.parse_args()

    if not args.source.exists():
        parser.error("source introuvable")
    if not SAFE_ID.fullmatch(args.cache_id):
        parser.error("cache-id : 1 à 23 caractères ASCII, chiffres, - ou _")
    if not (0 <= args.min_zoom <= args.max_zoom <= 20):
        parser.error("zooms attendus : 0 <= min <= max <= 20")
    if not (64 <= args.max_volume_mib <= MAX_VOLUME_MIB):
        parser.error("max-volume-mib doit être compris entre 64 et 2000 pour Tab5OS")

    iterator = make_iterator(args.source, args.min_zoom, args.max_zoom,
                             args.bbox)
    max_bytes = args.max_volume_mib * 1024 * 1024
    print("Analyse des tuiles…")
    plans = plan_volumes(iterator, max_bytes)
    total_tiles = sum(plan.count for plan in plans)
    total_bytes = sum(projected_size(plan.count, plan.data_bytes)
                      for plan in plans)
    print(f"{total_tiles:,} tuiles, {len(plans)} volume(s), "
          f"{total_bytes / (1024 ** 3):.2f} Gio")

    volume_index = 0
    previous = read_manifest(args.destination)
    completed: list[tuple[Path, Path]] = []
    writer: VolumeWriter | None = None
    written_in_volume = 0
    for tile in iterator(True):
        if writer is None:
            plan = plans[volume_index]
            path = output_path(args.destination, volume_index, len(plans))
            volume_name = (args.name if len(plans) == 1 else
                           f"{args.name} {volume_index + 1}/{len(plans)}")
            print(f"Écriture de {path}…")
            writer = VolumeWriter(path, plan, args.cache_id, volume_name)
            written_in_volume = 0
        writer.add(tile)
        written_in_volume += 1
        if written_in_volume == plans[volume_index].count:
            writer.finish(publish=False)
            completed.append((writer.temporary, writer.path))
            writer = None
            volume_index += 1
    if writer is not None:
        writer.finish(publish=False)
        completed.append((writer.temporary, writer.path))
    if volume_index != len(plans):
        raise ValueError("la seconde lecture ne correspond pas au plan initial")
    for temporary, destination in completed:
        os.replace(temporary, destination)
    commit_manifest(args.destination, [destination for _, destination in completed],
                    args.cache_id, previous)
    print("Terminé. Copier les .t5map dans /System/maps/offline sur la microSD.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
