TAB5OS RADIO CATALOGUE 0.8.24

Contents
- ../radios-internet.json: optional full catalogue.
- radios-XX.json: country subset identified by ISO country code.
- catalogue.csv: readable PC index (UTF-8, semicolon delimiter).
- index.json: counts, sizes and SHA-256 hashes.
- provenance.json: Radio Browser UUID, station homepage, source language and
  tags, API request URL, source check and retrieval timestamps for every stream.

Selection
A bounded number of top-voted candidates per country is selected from Radio
Browser, followed by round-robin selection among their source tags to vary
content. Equal country quotas apply where compatible streams are available.
No owner location, listening history or personal musical taste is used.
Category labels contain a country code and source tag; they do not claim to
provide a complete editorial classification.

Filters
MP3, AAC or AAC+; lastcheckok=1; hls=0; resolved HTTP/HTTPS stream URL;
no unresolved playlist URL or URL containing credentials. Entries are
deduplicated by URL and by normalized name within each country.
The name/style/url fields obey the importer's limits. All stations are enabled,
but none are preset as favourites. Maximum: 800 stations and 256 KiB per import file.

What is verified
Entries match the API snapshot and its reported codec and availability metadata.
Tests exercise the actual MicroPython importer with the full pack, repeated import
and preservation of an existing personal station. Native catalogue capacity is
checked. This is not a playback test of every stream on the physical Tab5.

Primary sources
https://www.radio-browser.info/
https://www.radio-browser.info/add
https://docs.radio-browser.info/
https://api.radio-browser.info/

Radio Browser dedicates its station metadata to the public domain. Station audio,
logos and websites retain their own rights. This pack redistributes metadata and
URLs only, not audio programmes. Homepage links in provenance.json are supplied
by the directory; they were not individually audited by Tab5OS.

Rebuilding (PC, Python 3 standard library)
From the Tab5OS sketch sources:
  python tools/build_radio_catalogs.py
Rebuilds offline from the supplied snapshot.
  python tools/build_radio_catalogs.py --refresh
Discovers servers and refreshes API metadata.
  python tools/radio-catalogs/verify_catalogs.py
Verifies the packs, provenance and importer.

Snapshot: tools/radio-catalogs/radio-browser-snapshot.json.gz in the sources.
It contains decoded API responses, queries and retrieval timestamps.
Rebuilding the same snapshot with the same quota reproduces the same packs.
Refreshing may change the selection and URLs as the directory changes; neither
operation edits the user's /radios.json or favourites on the SD card.
