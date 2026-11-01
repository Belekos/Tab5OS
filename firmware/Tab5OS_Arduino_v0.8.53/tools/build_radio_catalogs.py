#!/usr/bin/env python3
"""Build bounded, optional Tab5OS radio packs from a reproducible API snapshot.

No third-party package. By default, rebuild offline from the bundled snapshot.
Use --refresh to query Radio Browser again (stations may disappear/change).
The script never edits /radios.json or favourites on the user's SD card.
"""
from __future__ import annotations

import argparse
import collections
import concurrent.futures
import csv
import datetime as dt
import gzip
import hashlib
import io
import json
from pathlib import Path
import re
import time
import unicodedata
import urllib.parse
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "tools/radio-catalogs/radio-browser-snapshot.json.gz"
PACK = ROOT / "sdcard/Packs/radios-internet.json"
OUTPUT = ROOT / "sdcard/Packs/Radios"
COUNTRIES = """AR AT AU BD BE BG BR CA CH CL CN CO CR CU CZ DE DK DO DZ EC EE EG
ES FI FR GB GH GR HR HU ID IE IL IN IS IT JP KE KR LB LK LT LV MA MX MY NG
NL NO NP NZ PE PH PK PL PR PT RO RS RU SA SE SG SK SN TH TN TR TW TZ UA
UG US VN ZA""".split()
USER_AGENT = "Tab5OS-radio-catalog/0.8.24 (offline public-domain station packs)"
MAX_BYTES = 256 * 1024
MAX_STATIONS = 800
CODECS = {"MP3", "AAC", "AAC+"}


def json_bytes(value, pretty=False):
    return (json.dumps(value, ensure_ascii=False, indent=2 if pretty else None,
                       separators=None if pretty else (",", ":")) + "\n").encode("utf-8")


def csv_cell(value):
    # Directory metadata is untrusted input; do not create spreadsheet formulas.
    text = str(value)
    return "'" + text if text.startswith(("=", "+", "-", "@", "\t", "\r")) else text


def fetch(url):
    request = urllib.request.Request(url, headers={"User-Agent": USER_AGENT,
                                                   "Accept": "application/json"})
    with urllib.request.urlopen(request, timeout=25) as response:
        raw = response.read(2 * 1024 * 1024 + 1)
    if len(raw) > 2 * 1024 * 1024:
        raise ValueError("Radio Browser response exceeds 2 MiB")
    return raw


def refresh(destination):
    discovery = "https://all.api.radio-browser.info/json/servers"
    servers = json.loads(fetch(discovery))
    hosts = sorted({item.get("name", "") for item in servers
                    if re.fullmatch(r"[a-z0-9-]+\.api\.radio-browser\.info", item.get("name", ""))})
    if not hosts:
        raise RuntimeError("Radio Browser server discovery returned no valid host")

    def country_query(code):
        query = urllib.parse.urlencode({"countrycode": code, "hidebroken": "true",
                                       "order": "votes", "reverse": "true", "limit": 80})
        errors = []
        for host in hosts:
            url = "https://" + host + "/json/stations/search?" + query
            try:
                raw = fetch(url)
                records = json.loads(raw)
                if not isinstance(records, list) or len(records) > 80:
                    raise ValueError("Unexpected Radio Browser result")
                print(code, len(records), flush=True)
                # Requests are bounded and modestly spaced; no stream connections.
                time.sleep(0.3)
                return {"countrycode": code, "url": url,
                        "retrieved_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
                        "response_sha256": hashlib.sha256(raw).hexdigest(),
                        "records": records}
            except (OSError, ValueError) as error:
                errors.append(str(error))
        raise RuntimeError(code + ": " + "; ".join(errors))

    # Three catalogue requests maximum at a time, never probing station audio.
    with concurrent.futures.ThreadPoolExecutor(max_workers=3) as executor:
        queries = list(executor.map(country_query, COUNTRIES))
    snapshot = {"source": "https://www.radio-browser.info/", "discovery": discovery,
                "retrieved_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
                "user_agent": USER_AGENT, "queries": queries}
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(gzip.compress(json_bytes(snapshot), mtime=0))
    return snapshot


def clean(value, byte_limit):
    text = " ".join(str(value).split())
    text = "".join(c for c in text if unicodedata.category(c) not in ("Cc", "Cf"))
    while len(text.encode("utf-8")) > byte_limit:
        text = text[:-1]
    return text.strip()


def identity(value):
    return "".join(c for c in unicodedata.normalize("NFKD", value).casefold()
                   if c.isalnum())


def genre(record):
    # Keep actual source tags as catalogue data, without inventing station genres.
    tags = [clean(x, 48).replace("/", "-") for x in str(record.get("tags", "")).split(",")]
    ignored = {"mp3", "aac", "aac+", "radio", "fm", "am", "internet radio", "online", "webradio"}
    return next((x for x in tags if x and x.casefold() not in ignored
                 and not re.fullmatch(r"[0-9]+ ?(kbps|kbit/s|fm|am)", x.casefold())), "Radio")


def acceptable(record, code):
    codec = str(record.get("codec", "")).upper()
    url = str(record.get("url_resolved") or record.get("url") or "").strip()
    if (record.get("countrycode") != code or not record.get("lastcheckok")
            or record.get("hls") or codec not in CODECS):
        return False
    try:
        parsed = urllib.parse.urlsplit(url)
        if parsed.scheme not in ("http", "https") or not parsed.hostname or parsed.username or parsed.password:
            return False
    except ValueError:
        return False
    # Credentials, expiring URLs, HLS and unresolved playlist URLs are unsuitable.
    if (len(url.encode()) > 512 or any(ord(c) <= 32 for c in url)
            or parsed.path.lower().endswith((".m3u", ".m3u8", ".pls", ".asx"))
            or re.search(r"(?:^|[?&])(token|expires|signature|auth)=", parsed.query, re.I)):
        return False
    return bool(clean(record.get("name", ""), 128))


def select(snapshot, quota):
    selected = []
    seen_urls = set()
    seen_names = set()
    for query in snapshot["queries"]:
        code = query["countrycode"]
        buckets = collections.defaultdict(list)
        for record in query["records"]:
            if acceptable(record, code):
                buckets[genre(record)].append(record)
        # Round-robin by actual source tag avoids taking only one genre per country.
        # The API already sorts candidates by votes, not by the author's tastes.
        order = sorted(buckets, key=lambda g: (-max(r.get("votes", 0) for r in buckets[g]), g.casefold()))
        chosen = 0
        while chosen < quota and any(buckets.values()):
            for tag in order:
                if chosen >= quota:
                    break
                while buckets[tag]:
                    record = buckets[tag].pop(0)
                    url = str(record.get("url_resolved") or record["url"]).strip()
                    name = clean(record["name"], 128)
                    key = (code, identity(name))
                    if url in seen_urls or key in seen_names:
                        continue
                    seen_urls.add(url)
                    seen_names.add(key)
                    row = {"name": name, "style": code + "/" + tag, "url": url,
                           "enabled": True, "codec": str(record["codec"]).upper(),
                           "bitrate": int(record.get("bitrate") or 0)}
                    selected.append((row, record, query))
                    chosen += 1
                    break
    return sorted(selected, key=lambda item: (item[2]["countrycode"], item[0]["style"].casefold(),
                                              item[0]["name"].casefold()))


def write_catalog(path, rows):
    raw = json_bytes({"radios": rows}, pretty=True)
    if len(rows) > MAX_STATIONS or len(raw) > MAX_BYTES:
        raise ValueError("Pack exceeds Tab5OS import limit: " + str(path))
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(raw)
    return {"file": path.name, "stations": len(rows), "bytes": len(raw),
            "sha256": hashlib.sha256(raw).hexdigest()}


def build(snapshot, quota):
    selected = select(snapshot, quota)
    if len(selected) < 300:
        raise RuntimeError("Fewer than 300 compatible, distinct catalogue entries; refusing incomplete pack")
    rows = [item[0] for item in selected]
    countries = collections.defaultdict(list)
    for row, record, query in selected:
        countries[query["countrycode"]].append(row)
    stats = [write_catalog(PACK, rows)]
    for code, country_rows in sorted(countries.items()):
        stats.append(write_catalog(OUTPUT / ("radios-" + code + ".json"), country_rows))
    provenance = []
    for row, record, query in selected:
        provenance.append({"name": row["name"], "url": row["url"], "countrycode": query["countrycode"],
                           "stationuuid": record["stationuuid"], "homepage": record.get("homepage", ""),
                           "language": record.get("language", ""), "tags": record.get("tags", ""),
                           "codec": record["codec"], "hls": record["hls"],
                           "lastcheckok": record["lastcheckok"],
                           "lastchecktime_utc": record.get("lastchecktime_iso8601") or record.get("lastchecktime"),
                           "retrieved_utc": query["retrieved_utc"], "source_query": query["url"]})
    (OUTPUT / "provenance.json").write_bytes(json_bytes({"source": snapshot["source"],
        "retrieved_utc": snapshot["retrieved_utc"], "selection": "country-balanced, source-tag round-robin; top-voted candidates",
        "per_country_maximum": quota, "stream_tests_by_tab5os": False, "stations": provenance}, pretty=True))
    report = {"stations": len(rows), "countries": len(countries),
              "codecs": dict(collections.Counter(row["codec"] for row in rows)),
              "snapshot_sha256": hashlib.sha256(SOURCE.read_bytes()).hexdigest(),
              "retrieved_utc": snapshot["retrieved_utc"], "packs": stats}
    (OUTPUT / "index.json").write_bytes(json_bytes(report, pretty=True))
    with (OUTPUT / "catalogue.csv").open("w", encoding="utf-8-sig", newline="") as stream:
        writer = csv.writer(stream, delimiter=";")
        writer.writerow(("countrycode", "name", "category", "codec", "bitrate_kbps", "languages", "url", "homepage", "stationuuid", "source_check_utc"))
        for row, origin, query in selected:
            writer.writerow(tuple(csv_cell(value) for value in (query["countrycode"], row["name"], row["style"], row["codec"], row["bitrate"],
                             origin.get("language", ""), row["url"], origin.get("homepage", ""),
                             origin["stationuuid"], origin.get("lastchecktime_iso8601", ""))))
    print(json.dumps({k: v for k, v in report.items() if k != "packs"}, ensure_ascii=False, indent=2))
    print("Main pack:", stats[0]["bytes"], "bytes")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--refresh", action="store_true", help="fetch current catalogue metadata before rebuilding")
    parser.add_argument("--per-country", type=int, default=8)
    args = parser.parse_args()
    if not 1 <= args.per_country <= 10:
        parser.error("--per-country must be 1..10 (firmware limit: 800 stations)")
    snapshot = refresh(SOURCE) if args.refresh else json.loads(gzip.decompress(SOURCE.read_bytes()))
    build(snapshot, args.per_country)


if __name__ == "__main__":
    main()
