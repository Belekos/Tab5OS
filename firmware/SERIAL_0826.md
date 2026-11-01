# Série — Tab5OS 0.8.26

Réglages → Système → Série sélectionne **USB-C** ou **RS485**, puis le mode **Débogage** ou **Données**. Appliquer mémorise les paramètres en NVS uniquement lorsqu'ils changent. Le réglage initial est USB-C / Débogage. Aucune écriture périodique sur microSD.

- **USB-C** : périphérique USB Serial/JTAG intégré au P4, pour un câble de données relié à un ordinateur. Ouvrir son port série avec le logiciel du PC. Le débit et le format UART ne s'appliquent pas à cette liaison USB ; leurs champs sont désactivés.
- **RS485** : connecteur RS485 intégré de la Tab5, UART1, TX GPIO20, RX GPIO21, commande de direction GPIO34. Ne pas raccorder une liaison RS232 ou un UART TTL directement aux lignes A/B. Débits de 1 200 à 921 600 bauds ; formats 8N1, 8E1, 8O1, 8N2. La commande terminal accepte aussi un débit intermédiaire.
- **Débogage** : miroir des journaux applicatifs et de stdout/stderr vers le port choisi. L'UART0 de diagnostic reste disponible séparément. Sur le port choisi, la touche `m` demande les diagnostics mémoire uniquement en mode Débogage ; l'UART0 conserve cette commande dans les deux modes.
- **Données** : envoi/réception d'octets avec les commandes ci-dessous. Les journaux applicatifs et SDK ordinaires sont exclus de cette liaison. Les messages du boot ROM et d'une panne fatale peuvent encore apparaître au redémarrage : un protocole externe doit attendre le démarrage et prévoir son propre cadrage.

L'USB-A demeure l'hôte existant pour manettes, clavier, souris, stockage et RTL-SDR. Le service série n'installe aucun second hôte et ne reconfigure pas le contrôleur haute vitesse USB-A. Il n'utilise pas TinyUSB CDC : le périphérique matériel USB Serial/JTAG utilise le PHY pleine vitesse de l'USB-C.

## Terminal

```sh
serial                         # état, compteurs RX/TX, files et pertes
serial port usb                # ou rs485
serial mode data               # ou debug
serial baud 115200             # RS485 uniquement
serial frame 8N1               # RS485 : 8E1, 8O1, 8N2 aussi
serial ending crlf             # none, lf, crlf
serial send 'Bonjour'          # texte + fin de ligne configurée
serial write 'Bonjour'         # texte exact, sans fin ajoutée
cat /Documents/message.txt | serial write
serial write < /Documents/message.txt
serial read 4096 1000 > /Documents/reception.bin
serial listen 60000            # écoute 60 secondes ; Ctrl+C interrompt
serial clear                   # annule les files en attente
```

`read [octets [millisecondes]]` reçoit jusqu'au nombre indiqué pendant le délai indiqué : 8 192 octets / 1 000 ms par défaut. Un délai de 0 vide immédiatement les octets déjà disponibles. `listen [millisecondes]` continue jusqu'au délai (60 secondes par défaut, maximum une heure). Les redirections conservent les octets binaires ; l'affichage du Terminal sert au texte. `serial write` sans arguments lit son entrée standard. Les envois sont progressifs et interrompables ; si la file ne progresse plus pendant 10 secondes, la commande signale l'échec. Le succès signifie que les octets ont été acceptés par les files locales, pas qu'un équipement distant les a confirmés.

Le mode Données doit être choisi avant l'envoi. Les changements de paramètres sont refusés tant qu'un envoi applicatif reste en attente. `clear` ou « Annuler l'envoi » annule ce qui reste dans les files applicatives ; les octets déjà remis au pilote ou à l'équipement ne peuvent pas être retirés.

## Éditeur

Le bouton Enregistrer avec chevron ouvre **Fichier**, qui conserve Enregistrer et Enregistrer sous et ajoute **Envoyer par série** / **Annuler l'envoi**. Ctrl+S conserve l'enregistrement direct. L'envoi prend la sélection lorsqu'elle existe, sinon tout le document, jusqu'à 512 Kio. Il copie un instantané en PSRAM ; modifier le document ensuite ne change pas l'envoi. Le texte et ses fins de ligne restent inchangés et le presse-papiers n'est pas modifié. Le mode et le port se choisissent dans Réglages.

## Ressources et vérification

Deux files applicatives fixes de 8 Kio en PSRAM. Le pilote USB utilise 4 Kio RX / 2 Kio TX ; le pilote RS485 utilise les mêmes limites. Un seul envoi d'éditeur peut ajouter un instantané borné à 512 Kio, libéré et effacé après transmission ou annulation. Le worker utilise une pile réservée de 4 Kio, traite au maximum 512 octets par passage, et attend 5 ms entre passages. Les écritures USB utilisent un délai nul : un ordinateur absent ou un port fermé n'arrête pas l'interface. Les octets reçus lorsque la file est pleine sont comptés dans `dropped`.

Le service redirige stdout/stderr à l'initialisation vers un VFS de diagnostic dédié, afin d'éviter la duplication USB secondaire configurée dans le SDK. Ce VFS et le wrapper `Tab5DebugSerial` conservent l'UART0 et utilisent le même miroir borné, actif uniquement en Débogage. Aucun remplacement récursif de `esp_log_set_vprintf`.

**Pas de test physique réalisé** : compatibilité du câble/port PC, RS485, débit soutenu, coexistence radio et états de veille restent à vérifier sur la tablette. Une connexion USB détectée indique la présence du bus USB, pas une confirmation du programme distant.

Sources primaires : [HAL RS485 officiel M5Stack](https://github.com/m5stack/M5Tab5-UserDemo/blob/main/platforms/tab5/main/hal/components/hal_rs485.cpp), [console ESP-IDF 5.5.4](https://github.com/espressif/esp-idf/blob/v5.5.4/components/esp_vfs_console/vfs_console.c). Les en-têtes installés du SDK P4 3.3.9 confirment `CONFIG_USJ_ENABLE_USB_SERIAL_JTAG` et `CONFIG_ESP_CONSOLE_SECONDARY_USB_SERIAL_JTAG` ; le routage physique USB-A/USB-C et son schéma officiel sont référencés dans `STOCKAGE_USB.md`.
