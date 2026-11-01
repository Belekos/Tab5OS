#!/usr/bin/env python3
"""Verify delivered packs against the snapshot and the actual MicroPython importer."""
import collections
import gzip
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import sys
import types

# Loading the real SD module must not put CPython bytecode on the SD image.
sys.dont_write_bytecode = True

ROOT = Path(__file__).resolve().parents[2]
PACK = ROOT / "sdcard/Packs/radios-internet.json"
CATALOGS = ROOT / "sdcard/Packs/Radios"


def read(path):
    return json.loads(Path(path).read_text(encoding="utf-8"))


def main():
    snapshot_path = ROOT / "tools/radio-catalogs/radio-browser-snapshot.json.gz"
    snapshot = json.loads(gzip.decompress(snapshot_path.read_bytes()))
    snapshot_records = {r["stationuuid"]: r for q in snapshot["queries"] for r in q["records"]}
    rows = read(PACK)["radios"]
    index = read(CATALOGS / "index.json")
    provenance = read(CATALOGS / "provenance.json")["stations"]
    assert 300 <= len(rows) <= 800
    assert len(rows) == index["stations"] == len(provenance)
    assert len({r["url"] for r in rows}) == len(rows)
    assert not any("favorite" in key.casefold() for row in rows for key in row)
    assert hashlib.sha256(snapshot_path.read_bytes()).hexdigest() == index["snapshot_sha256"]
    grouped = []
    for summary in index["packs"]:
        path = PACK if summary["file"] == PACK.name else CATALOGS / summary["file"]
        content = path.read_bytes()
        assert len(content) == summary["bytes"] <= 262144
        assert hashlib.sha256(content).hexdigest() == summary["sha256"]
        pack_rows = json.loads(content)["radios"]
        assert len(pack_rows) == summary["stations"] <= 800
        if path != PACK:
            grouped.extend(pack_rows)
    assert grouped == rows
    for row, origin in zip(rows, provenance):
        source = snapshot_records[origin["stationuuid"]]
        assert row["url"] == origin["url"] == (source.get("url_resolved") or source["url"]).strip()
        assert row["codec"] in ("MP3", "AAC", "AAC+")
        assert source["lastcheckok"] == 1 and source["hls"] == 0
        assert row["style"].startswith(source["countrycode"] + "/")
        assert row["enabled"] is True

    # Load and exercise the real importer, replacing only SD I/O and factory constants.
    fs = types.ModuleType("tab5fs")
    factory = types.ModuleType("tab5factory")
    factory.VERSION = "test"
    factory.FILES = factory.PREVIOUS = ()
    existing = []
    fs.exists = lambda path: path == "/radios.json" and bool(existing)
    fs.read_json = lambda path: {"radios": existing} if path == "/radios.json" else read(path)
    fs.stat = lambda path: (0, len(json.dumps({"radios": existing}).encode())) if path == "/radios.json" else (0, Path(path).stat().st_size)
    sys.modules["tab5fs"] = fs
    sys.modules["tab5factory"] = factory
    spec = importlib.util.spec_from_file_location("actual_tab5workshop", ROOT / "sdcard/MicroPython/lib/tab5workshop.py")
    workshop = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(workshop)
    imported = workshop.read_radios(str(PACK))
    assert len(imported) == len(rows)
    merged, added, updated, old = workshop.radio_preview(str(PACK))
    assert added == len(rows) and updated == 0
    existing.extend(merged["radios"])
    merged, added, updated, old = workshop.radio_preview(str(PACK))
    assert added == 0 and updated == len(rows) and len(merged["radios"]) == len(rows)
    custom = {"name": "Existing user station", "url": "https://example.invalid/personal", "style": "Personal", "enabled": False}
    existing.append(custom)
    merged, added, updated, old = workshop.radio_preview(str(PACK))
    assert len(merged["radios"]) == len(rows) + 1 and custom in merged["radios"]
    # Firmware capacity must match the published catalogue, not truncate it at 256.
    firmware = (ROOT / "Tab5Audio.cpp").read_text(encoding="utf-8-sig")
    match = re.search(r"MAX_RADIO_STATIONS\s*=\s*(\d+)\s*;", firmware)
    assert match and int(match.group(1)) >= len(rows), "Native radio capacity is smaller than the delivered catalogue"
    report = {"status": "PASS", "stations": len(rows), "countries": index["countries"],
              "pack_bytes": PACK.stat().st_size, "unique_urls": len({r["url"] for r in rows}),
              "codecs": dict(collections.Counter(r["codec"] for r in rows)),
              "source_check_oldest": min(r["lastchecktime_utc"] for r in provenance),
              "source_check_newest": max(r["lastchecktime_utc"] for r in provenance),
              "retrieved_utc": index["retrieved_utc"],
              "checks": ["pack/source URL identity", "JSON and UTF-8 bounds", "per-country pack union",
                         "no duplicate URL", "no preset favourites", "real importer and repeat import",
                         "existing user station retained", "native firmware catalogue capacity"],
              "stream_audio_tested": False, "hardware_playback_tested": False}
    report_path = Path(__file__).with_name("verification.json")
    report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
