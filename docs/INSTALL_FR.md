# Installer Tab5OS 0.8.53

Tab5OS utilise le M5Stack Tab5 / ESP32-P4 avec 32 Mio de PSRAM et 16 Mio de Flash.

## Téléchargements

| Fichier | Utilisation |
| --- | --- |
| [Firmware OTA](../downloads/Tab5OS_0.8.53_OTA.bin) | Mettre à jour Tab5OS depuis son Explorateur |
| [Installation initiale 16 Mio](../downloads/Tab5OS_0.8.53_initial_install_16MB.bin) | Installer l’image fusionnée à l’adresse `0x0` |
| [Paquet SD complet](../downloads/Tab5OS_0.8.53_SD.zip) | Applications ELF/Python, polices, langues, claviers et référence Factory |
| [SHA256SUMS.txt](../downloads/SHA256SUMS.txt) | Vérifier les trois téléchargements |

**L’installation initiale remplace toute la Flash du P4, y compris les réglages et les clés internes. Pour mettre à jour une installation existante, utiliser le firmware OTA.** Sauvegarder les documents, réglages et exports portables du coffre avant l’installation. Maintenir l’alimentation pendant l’écriture. Le coprocesseur ESP32-C6 conserve son firmware.

Pour vérifier un téléchargement dans PowerShell, calculer son empreinte avec `Get-FileHash -Algorithm SHA256 .\Tab5OS_0.8.53_SD.zip` et la comparer à `SHA256SUMS.txt`. Sous Linux, placer les trois fichiers et les empreintes dans le même dossier, puis lancer `sha256sum -c SHA256SUMS.txt`.

## Première installation

1. Décompresser le paquet SD. Sur une microSD FAT32, copier le **contenu** de son dossier `sdcard` à la racine : `Applications`, `MicroPython`, `System`, `Factory`, etc.
2. Installer Python 3.10 ou ultérieur et esptool 5 :

   ```powershell
   python -m pip install "esptool>=5,<6"
   ```

3. Brancher le port USB-C du Tab5 avec un câble de données. Maintenir Reset environ deux secondes, jusqu’au clignotement rapide de la LED verte, puis relâcher.
4. Ouvrir un terminal dans le dossier du BIN d’installation initiale. Remplacer `COM5` par le port du Tab5, puis exécuter :

   ```powershell
   python -m esptool --chip esp32p4 --port COM5 write-flash 0x0 Tab5OS_0.8.53_initial_install_16MB.bin
   ```

5. Attendre la fin de l’écriture et de la vérification, puis redémarrer avec la microSD préparée. Choisir la langue et configurer le Wi-Fi dans **Réglages**.

Sous Linux ou macOS, utiliser `python3` si nécessaire et le port correspondant, par exemple `/dev/ttyACM0`. L’image fusionnée contient le bootloader, la table de partitions et l’application ; elle s’écrit en une seule opération à `0x0`.

## Mettre à jour Tab5OS

1. Copier [Tab5OS_0.8.53_OTA.bin](../downloads/Tab5OS_0.8.53_OTA.bin) sur la microSD.
2. Dans l’Explorateur, sélectionner ce fichier puis **Actions → Installer un firmware**.
3. Choisir **Vérifier** et comparer le SHA-256 affiché à [SHA256SUMS.txt](../downloads/SHA256SUMS.txt).
4. Choisir **Installer**, attendre la réussite, puis **Redémarrer**.
5. Actualiser les ressources SD suivant la procédure ci-dessous.

La mise à jour conserve la table de partitions et les réglages internes NVS. Le BIN OTA est destiné à cette procédure ; ne pas l’écrire à `0x0`.

## Actualiser une microSD existante

Le [paquet SD complet](../downloads/Tab5OS_0.8.53_SD.zip) contient aussi `Factory/Tab5OS-0.8.53`, la référence utilisée par l’outil de réparation.

1. Sauvegarder les fichiers personnels, scripts adaptés, réglages, clés, archives, jeux et sauvegardes.
2. Copier `sdcard/Factory/Tab5OS-0.8.53` du paquet vers `/Factory/Tab5OS-0.8.53` sur la carte.
3. Depuis le paquet, actualiser `MicroPython/lib/tab5factory.py`, `tab5workshop.py`, `tab5fs.py`, `tab5gui.py`, `tab5ui.py`, `tab5apps.py` et `MicroPython/update_sd.py`, après avoir conservé leurs éventuelles adaptations.
4. Redémarrer, ouvrir `/MicroPython/update_sd.py` et choisir **Réparer et mettre à jour**.
5. Redémarrer pour charger les ressources actualisées.

**Conserver le dossier `/System` personnel.** La réparation normale préserve les ressources personnalisées et les réglages valides ; l’action distincte **Restaurer les fichiers modifiés** les remplace après confirmation. Le [guide de réparation SD](../firmware/REPARATION_SD.md) détaille ces règles.

Les [11 applications ELF](../firmware/Tab5OS_Arduino_v0.8.53/sdcard/Applications/) et les [scripts Python](../firmware/Tab5OS_Arduino_v0.8.53/sdcard/MicroPython/) sont également consultables directement. Utiliser le ZIP pour préparer une carte avec toutes les ressources. Les guides [Wikipédia](../firmware/WIKIPEDIA.md), [Cartes](../firmware/CARTES.md) et [Applications](../firmware/APPLICATIONS.md) décrivent la préparation des archives, tuiles et données de jeux.

Pour reconstruire le firmware, suivre le [guide de compilation](../firmware/COMPILATION.md).

Références officielles : [installation d’esptool](https://docs.espressif.com/projects/esptool/en/latest/esp32/installation.html), [écriture d’une image](https://docs.espressif.com/projects/esptool/en/latest/esp32p4/esptool/basic-commands.html), [mode de téléchargement Tab5](https://docs.m5stack.com/en/uiflow2/Tab5/program).
