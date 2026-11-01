RADIO / VIDEO PACKS - Tab5OS 0.8.24

FRANÇAIS

radios-internet.json est un catalogue facultatif de plusieurs centaines de
stations. Radios/index.json donne le nombre exact, les pays et les empreintes.
Radios/radios-XX.json contient la sélection du pays identifié par son code ISO.
Les noms et les catégories sont des données du catalogue : code pays / mot-clé
de la source. Aucune station n'est imposée comme favori ni lancée automatiquement.

Pour une installation vierge, copier le pack voulu à la racine de la carte SD
sous le nom radios.json. Si /radios.json existe déjà, conserver ce fichier :
utiliser l'application « Importer des radios », si elle est installée, puis
l'import de fichier JSON pour fusionner les stations par adresse. Il est aussi
possible de fusionner les tableaux radios sur PC. Ne pas remplacer un catalogue
personnel sans en conserver une copie. Fermer et rouvrir Média pour recharger.

Limites : 800 stations au total après fusion et 256 Kio par fichier JSON importé.
Le grand pack et les packs par pays proposent les mêmes stations ; importer les
deux n'ajoute pas de doublons d'adresse. Les fichiers provenance.json, index.json
et catalogue.csv servent à documenter les packs et ne sont pas des imports radio.

Source : Radio Browser, annuaire public communautaire. Les adresses de flux,
formats MP3/AAC et états de disponibilité proviennent de son API. La date exacte
de collecte et le dernier contrôle rapporté pour chaque station figurent dans
Radios/provenance.json. Les flux n'ont pas tous été ouverts ou écoutés par Tab5OS.
Leur fonctionnement sur la tablette, les restrictions géographiques et leur
disponibilité ultérieure restent variables. Voir Radios/README.txt.

videos-mjpeg.json reste un exemple de deux clips HTTP de l'archive publique
FFmpeg, en AVI Motion-JPEG 320x240 avec audio PCM. Ce ne sont pas des chaînes TV
en direct. Aucun contenu audio ou vidéo n'est distribué dans ces packs.
Source : https://samples.ffmpeg.org/V-codecs/MJPEGs/

Le lecteur accepte également le multipart MJPEG HTTP (format=mjpeg), par exemple
le serveur Picamera2 d'un appareil personnel. HLS, RTSP, les pages YouTube et les
vidéos H.264/MP4 ne sont pas décodés par ce lecteur.
https://github.com/raspberrypi/picamera2/blob/main/examples/mjpeg_server.py

ENGLISH

radios-internet.json is an optional catalogue of several hundred stations.
Radios/index.json provides exact counts, country codes and SHA-256 hashes.
Radios/radios-XX.json is the country subset, identified by its ISO country code.
Station names and source tags are catalogue data, not interface translations.
No preset favourites, automatic playback, or owner's listening preferences.

On a fresh SD card, copy the desired pack to /radios.json. If /radios.json already
exists, keep it: use the file import in the Import radios application, if
installed, to merge stations by URL, or merge the radios arrays on a PC. Keep a
backup before replacing a personal catalogue. Reopen Media to reload the list.

The combined catalogue is limited to 800 stations and import files to 256 KiB.
The full pack and country packs contain the same stations; importing both does
not duplicate URLs. provenance.json, index.json and catalogue.csv are reference
files, not radio imports.

Source: Radio Browser's public community directory. Its API supplies the stream
URLs, codecs and reported availability. Per-station check timestamps and retrieval
timestamps are in Radios/provenance.json. These streams were not all opened or
listened to by Tab5OS; hardware playback, geographical availability and future
availability are not guaranteed. See Radios/README.txt for provenance and rebuilds.

videos-mjpeg.json contains two example HTTP clips from the public FFmpeg sample
archive: AVI Motion-JPEG 320x240 with PCM audio, not live television. No audio or
video content is redistributed. Personal multipart HTTP MJPEG endpoints are also
supported; HLS, RTSP, YouTube pages and H.264/MP4 are not decoded by this player.
