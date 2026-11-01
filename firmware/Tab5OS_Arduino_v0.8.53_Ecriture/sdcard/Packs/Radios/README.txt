CATALOGUE RADIO TAB5OS 0.8.24

Contenu / Contents
- ../radios-internet.json : catalogue complet facultatif / optional full pack.
- radios-XX.json : sélection par pays ISO / ISO country subset.
- catalogue.csv : index lisible sur PC / readable PC index (UTF-8, delimiter ;).
- index.json : nombres, tailles et SHA-256 / counts, sizes and hashes.
- provenance.json : UUID Radio Browser, site officiel de chaque station, langue,
  mots-clés d'origine, URL de requête, dates de collecte et de contrôle de la source.
  Radio Browser UUID, station homepage, source language/tags, API request URL,
  source check and retrieval timestamps for every selected stream.

Sélection / Selection
Un nombre borné de candidats par pays, classés par votes dans Radio Browser.
Sélection tournante entre leurs mots-clés pour varier les contenus ; quota égal
par pays lorsque des flux compatibles sont disponibles. Pas de géolocalisation
du propriétaire et aucun historique ni goût musical personnel utilisé.
Les catégories affichées reprennent le code du pays et un mot-clé de la source.
Elles ne prétendent pas être une classification éditoriale exhaustive.

Bounded, top-voted candidates per country, then round-robin selection among their
source tags. Equal country quotas where compatible streams are available. No
owner location, listening history or personal musical taste is used. Category
labels contain a country code and source tag, not a complete editorial taxonomy.

Filtres / Filters
MP3, AAC ou AAC+ ; lastcheckok=1 ; hls=0 ; URL HTTP/HTTPS résolue ; aucune URL de
playlist non résolue ou contenant des identifiants. Doublons retirés par URL et
par nom normalisé dans un même pays. Les champs name/style/url respectent les
limites de l'importeur. Toutes les stations sont disponibles mais aucune n'est
un favori. La liste est plafonnée à 800 stations et le pack à 256 Kio.

MP3/AAC/AAC+, successful source check, no HLS, resolved HTTP/HTTPS stream URL,
no unresolved playlist URL or credentials. Deduplicated by URL and normalized
name within each country. Import fields obey the firmware limits. No favourites
are preset. Maximum 800 stations and 256 KiB per import file.

Ce qui est vérifié / What is verified
Les entrées correspondent au snapshot de l'API, leurs formats et leur dernier
état sont ceux rapportés par Radio Browser. Le vrai importeur MicroPython est
testé avec le grand pack, sa réimportation et la conservation d'une station
personnelle préexistante. Le lecteur natif accepte la capacité du pack.
Cela ne constitue pas un test d'écoute de toutes les stations sur le Tab5.

Entries match the API snapshot and its codec/availability metadata. Tests exercise
the actual MicroPython importer, repeated import and preservation of an existing
personal station. Native catalogue capacity is checked. This is not a playback
test of every stream on the physical Tab5.

Sources primaires / Primary sources
https://www.radio-browser.info/
https://www.radio-browser.info/add
https://docs.radio-browser.info/
https://api.radio-browser.info/

Radio Browser place les informations de l'annuaire dans le domaine public ; les
programmes audio, logos et sites des stations conservent leurs propres droits.
Ce pack redistribue uniquement des métadonnées et adresses, aucun programme audio.
Radio Browser dedicates its station metadata to the public domain. Station audio,
logos and websites retain their own rights. This pack contains metadata and links,
not audio programmes. Homepage links in provenance.json are supplied by the
directory; they were not individually audited by Tab5OS.

Reproduction / Rebuilding (PC Python 3, bibliothèque standard / standard library)
Depuis les sources Tab5OS / From the Tab5OS sketch sources:
  python tools/build_radio_catalogs.py
Reconstruit sans réseau depuis le snapshot fourni / offline rebuild.
  python tools/build_radio_catalogs.py --refresh
Découvre les serveurs puis actualise les métadonnées / discover and refresh API data.
  python tools/radio-catalogs/verify_catalogs.py
Vérifie packs, provenance et importeur / verify packs, provenance and importer.

Snapshot : tools/radio-catalogs/radio-browser-snapshot.json.gz dans les sources.
Il contient les réponses décodées, les requêtes et leurs dates de collecte.
Le même snapshot et le même quota produisent les mêmes packs. Une actualisation
change les adresses et la sélection en fonction de l'annuaire, sans modifier le
fichier /radios.json ni les favoris d'un utilisateur.

The source snapshot contains decoded API responses, queries and retrieval times.
Rebuilding the same snapshot with the same quota reproduces the packs. Refresh
may change selection and URLs as the directory changes; neither operation edits
the user's /radios.json or favourites on the SD card.
