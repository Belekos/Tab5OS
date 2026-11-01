# SDK mémoire : décision pour 0.8.47

Le SDK utilisé est celui du paquet Arduino M5Stack 3.3.9, variante P4 antérieure à V3 (`esp32p4_es-libs`). Son fichier `versions.txt` donne IDF `v5.5.4 / 735507283d`, lib-builder `43a8f6d`, Arduino `6cb835025`. Les archives sont contrôlées contre le verrou de la livraison précédente. Aucune définition d'en-tête ne prétend activer une fonction absente des bibliothèques précompilées.

Le `sdkconfig.h` DIO effectif active PSRAM HEX 200 MHz, CPU 360 MHz, cache L2 256 Kio et ligne 128 octets. Il n'active ni XiP depuis la PSRAM ni l'initialisation PSRAM précoce. Le code Arduino initialise la PSRAM ultérieurement. Les variantes DIO/QIO distribuées ne fournissent pas une variante XiP prête à l'emploi.

Dans le Kconfig P4 de la révision IDF exacte, XiP exige `SPIRAM_BOOT_INIT` et sélectionne `SPIRAM_FETCH_INSTRUCTIONS`, `SPIRAM_RODATA` et `SPIRAM_FLASH_LOAD_TO_PSRAM`. Une activation cohérente demande donc une reconstruction du SDK et du démarrage avec les mêmes composants ; un `-D` dans le sketch ne convient pas. Les données d'instructions/lecture seule sont alors copiées en PSRAM. Dans l'ELF 0.8.46, texte et rodata représentent 5 626 610 octets, auxquels s'ajoutent notamment 312 988 octets d'informations d'exception et les alignements. Cette charge réduit le tas PSRAM disponible et augmente le trafic sur son bus. Elle ne garantit pas une amélioration de l'affichage sans mesure matérielle.

L'auto-suspension Flash ne doit pas être activée par supposition : elle dépend de la Flash et de l'implémentation P4. L'incident Espressif IDFGH-17985 décrit une sélection possible sans suspension effective et des défauts d'accès PSRAM. Une migration SDK de ce type réclame la validation de la révision matérielle, des écritures NVS/OTA, de la persistance et de la lecture DSI concurrente.

Décision : conserver ici le SDK et les partitions authentifiés, corriger les écritures et traitements inutiles dans l'OS, et rendre les causes mesurables. XiP reste une qualification séparée, sans faux succès annoncé à partir d'un simple changement d'en-tête. Le second cœur n'annule pas à lui seul les sections critiques Flash qui arrêtent les deux cœurs.

Sources primaires examinées :

- `https://raw.githubusercontent.com/espressif/esp-idf/735507283d/components/esp_psram/esp32p4/Kconfig.spiram`
- `https://raw.githubusercontent.com/espressif/esp-idf/735507283d/components/spi_flash/spi_flash_os_func_app.c`
- `https://raw.githubusercontent.com/espressif/esp32-arduino-lib-builder/43a8f6d/build.sh`
- `https://github.com/espressif/esp-idf/issues/18846`

Les sources exactes et leurs empreintes sont conservées dans le relevé d'analyse du SDK. Aucune compilation d'un SDK XiP ni aucun essai physique n'est revendiqué.
