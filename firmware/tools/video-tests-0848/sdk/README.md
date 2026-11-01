# Sources de référence des tests JPEG

`jpeg_decode.c`, `jpeg_parse_marker.c` et `jpeg_private.h` proviennent de :
https://github.com/espressif/esp-idf/tree/v5.5.4/components/esp_driver_jpeg

`jpeg_types.h` est l'entête HAL du SDK ESP32-P4 ES fourni par M5Stack 3.3.9, correspondant à ESP-IDF 5.5.4. Les mentions de copyright et licences sont conservées dans les fichiers.

Les scripts extraient les fonctions CPU et la structure de données pour les tests hôte. Ces copies de référence ne remplacent pas le pilote compilé dans le firmware. Les empreintes exactes sont conservées dans le rapport `video-jpeg-probe-0848.json`.
