Offline place search — Tab5OS

Data: GeoNames, https://www.geonames.org/
Source: https://download.geonames.org/export/dump/cities1000.zip
License: Creative Commons Attribution 4.0 (CC BY 4.0)
https://creativecommons.org/licenses/by/4.0/

This database was converted and indexed by Tab5OS. It contains the source's
worldwide populated places above 1,000 inhabitants and administrative seats,
with canonical and alternate names. It is not an offline address, street or
exhaustive point-of-interest database. Coverage and names follow the source;
GeoNames data can be incomplete or inaccurate.

cities.json records the download URL, source/output/generator/folding-table SHA-256 hashes, date,
record counts and conversion format. Rebuild with tools/maps-gazetteer/build.py
and a newly downloaded GeoNames cities1000.zip. The tool does not download data.
Copy cities.t5geo to this directory with the OS stopped or SD ejected.

Search uses a bounded 4 KiB read cache and eight ranked results. The database
is not loaded into RAM. Names are compared without common Latin accents and
case, with prefix matching and alternate names. Country codes help distinguish
homonyms. Common prefixes may be capped; refine the query in that case.
