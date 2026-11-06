RADIO / VIDEO PACKS - Tab5OS 0.8.24

radios-internet.json is an optional catalogue of several hundred stations.
Radios/index.json provides exact counts, country codes and SHA-256 hashes.
Radios/radios-XX.json is the country subset, identified by its ISO country code.
Station names and categories are catalogue data: country codes and source tags.
No stations are preset as favourites or played automatically, and the owner's
listening preferences are not used.

On a fresh SD card, copy the desired pack to the card root as radios.json.
If /radios.json already exists, keep it: use JSON file import in the Import radios
application, if installed, to merge stations by URL. You can also merge the
radios arrays on a PC. Keep a backup before replacing a personal catalogue.
Close and reopen Media to reload the list.

The combined catalogue is limited to 800 stations and import files to 256 KiB.
The full pack and country packs contain the same stations; importing both does
not duplicate URLs. provenance.json, index.json and catalogue.csv document the
packs and are not radio imports.

Source: Radio Browser's public community directory. Its API supplies the stream
URLs, MP3/AAC formats and reported availability. Exact retrieval timestamps and
the last reported check for each station are in Radios/provenance.json.
These streams were not all opened or listened to by Tab5OS. Playback on the
tablet, geographical restrictions and future availability can vary.
See Radios/README.txt for provenance and rebuild instructions.

videos-mjpeg.json contains two example HTTP clips from the public FFmpeg sample
archive: AVI Motion-JPEG 320x240 with PCM audio. They are not live TV channels.
No audio or video content is distributed in these packs.
Source: https://samples.ffmpeg.org/V-codecs/MJPEGs/

The player also supports multipart HTTP MJPEG (format=mjpeg), such as a
Picamera2 server on your own device. HLS, RTSP, YouTube pages and H.264/MP4
videos are not decoded by this player.
https://github.com/raspberrypi/picamera2/blob/main/examples/mjpeg_server.py
