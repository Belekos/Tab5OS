# Accès matériel — Tab5OS 0.8.9

Le firmware complet, une application `.app.elf` et un script MicroPython n’ont pas le même environnement. L’ABI documentée est extensible avec les sources fournies ; aucun export absent ne doit être présenté comme disponible.

| Fonction | Application C `.app.elf` | MicroPython fourni | Firmware C/C++ complet |
|---|---|---|---|
| GPIO externes, PWM, ADC | Callback ABI 3 `hardware` | `machine.Pin`, `PWM`, `ADC` | Arduino / ESP-IDF |
| I²C, SPI, série externes | Callback ABI 3 | `machine.I2C`, `SPI`, `UART` | Pilotes du SDK |
| Alimentation 5 V du port externe | Opération 40 | `tab5.external_power()` | M5Unified |
| Écran, boutons, audio | ABI 2/3, surfaces et PCM gérés par l’OS | Pont `tab5ui`, API existantes | Pilotes de l’OS |
| Fichiers | Lecture ABI 1 ; sauvegardes de jeux ABI 2/3 | `open`, `tab5fs` | Arduino FS / POSIX |
| Réseau via C6 | Pas d’API réseau générale exportée dans l’ABI | Services réseau existants de Tab5OS | ESP-Hosted / Wi-Fi |
| GPIO internes du C6, BLE, Thread, Zigbee | Non exportés | Non exportés | Nécessite un firmware C6/protocole adaptés |
| Interruptions GPIO, RMT, DMA ou autres pilotes bruts | Non exportés | Non exportés par ce module `machine` | À intégrer avec ESP-IDF |

Le C6 est un processeur compagnon relié au P4 par SDIO. Les numéros passés à `machine.Pin` et à l’ABI sont ceux du **P4**, pas du C6. Recompiler seulement une application ne change pas le firmware du C6. Les broches et bus internes utilisés par l’affichage, la SD, le tactile, l’alimentation ou la liaison radio restent réservés tant que l’OS fonctionne. Pour une intégration nécessitant ces pilotes, modifier le firmware complet et organiser leur propriété ; le chargeur d’applications n’importe pas arbitrairement les symboles du SDK.

## Propriété et durée de vie

Une application Python et une application C peuvent coexister. Le courtier `Tab5Hardware` réserve les broches et les bus à leur utilisateur et renvoie `-EBUSY` si une autre application les possède. `deinit` ou le retour effectif du programme libère ses ressources. Une demande d’arrêt est coopérative : elle n’interrompt pas brutalement une transaction matérielle. Pas de callbacks ni de threads conservés après le retour de l’application.

Broches P4 proposées par cette version : **2, 3, 4, 5, 6, 7, 16, 17, 18, 19, 20, 21, 34, 35, 37, 38, 45, 47, 48, 51, 52, 53, 54**. Vérifier le connecteur correspondant et l’affectation physique dans le [brochage M5Stack Tab5](https://docs.m5stack.com/en/core/Tab5). Certaines sont partagées avec les fonctions du connecteur d’extension/RS485 ; la réservation logicielle ne détecte pas le câblage externe.

L’ADC n’accepte que les broches disposant réellement d’un canal analogique selon le SDK P4 compilé. `Pin.deinit()`, `PWM.deinit()` et `ADC.deinit()` remettent la broche en entrée ; pour une broche appartenant à un bus, fermer le bus. Un objet Pin utilisé comme paramètre d’un bus doit être réservé par la même application.

## MicroPython

```python
from machine import I2C, UART, ADC, pin_available

print(pin_available(53))  # 0 réservé, 1 disponible/à nous, 2 utilisé par une autre app
bus = I2C(1, sda=53, scl=54, freq=100000)
try:
    print(bus.scan())
finally:
    bus.deinit()

serial = UART(1, 115200, tx=6, rx=7)
try:
    message = b'Hello\r\n'
    sent = 0
    while sent < len(message):
        sent += serial.write(message[sent:])
        import time
        time.sleep_ms(1)
finally:
    serial.deinit()
```

UART : 8N1, lecture non bloquante, `write` retourne le nombre d’octets acceptés ; gérer les écritures partielles, `any()` et `txdone()`. Le débit est entre 1 200 et 1 000 000 bauds. SPI : contrôleur logique 2, 8 bits MSB, 1 kHz à 10 MHz, mode 0 à 3. I²C : identifiant logique 1, contrôleur matériel externe 0, 10 kHz à 1 MHz, temporisation 100 ms. Le bus I²C interne de l’OS n’est pas réinitialisé.

`ADC.read()` donne une valeur 12 bits, `read_u16()` la ramène à 16 bits, `read_uv()` utilise la conversion millivolts du SDK. Les classes fournies sont un sous-ensemble explicite de l’API MicroPython, pas une promesse de compatibilité avec toutes ses bibliothèques matérielles.

## ABI 3 pour C

Importer seulement `tab5_app_api_v3()` depuis `Tab5AppApi.h` et vérifier `base.base.version == 3` et `base.base.size >= sizeof(*api)`. Les API 1/2 restent compatibles pour les anciennes applications. Le callback est :

```c
int hardware(int op, int a, int b, int c,
             const void *input, size_t length,
             void *output, size_t capacity);
```

Résultat négatif : `-errno`. Au plus 4 096 octets en entrée/sortie par appel ; pointeur obligatoire si longueur non nulle. Toutes les opérations s’exécutent depuis le worker de l’application.

| Opération | Paramètres / résultat |
|---|---|
| 2 | a=GPIO, b=entrée 0/sortie 1/open-drain 2, c=sans pull 0/up 1/down 2 ; réserve la broche |
| 3 / 4 | Lire GPIO a / écrire GPIO a avec b booléen |
| 5 | a=GPIO ADC, b=atténuation 0..3, c=0 valeur brute / c=1 millivolts ; réserve au premier appel |
| 6 / 7 | PWM : a=GPIO, b=1..40 000 Hz, c=rapport 0..65 535 / libérer PWM a |
| 9 | Libérer la broche a (GPIO, PWM ou ADC) |
| 10 | Ouvrir I²C : a=SDA, b=SCL, c=fréquence |
| 11 | Écrire I²C : a=adresse 8..119, b=STOP booléen, input/length ; retourne le nombre écrit |
| 12 | Lire I²C : a=adresse, b=STOP, output/capacity <=128 ; retourne le nombre lu |
| 13 | Fermer I²C |
| 20 | Ouvrir SPI : a=SCK, b=MOSI, c=MISO ; CS géré séparément par GPIO |
| 21 | Transférer SPI : a=fréquence, b=mode ; length==capacity ; retourne la longueur |
| 22 | Fermer SPI |
| 30 | Ouvrir UART : a=TX, b=RX, c=bauds |
| 31 / 32 | Écrire input/length (peut être partiel) / lire output/capacity |
| 33 / 34 / 35 | Fermer UART / nombre d’octets disponibles / TX fini (0 ou 1) |
| 36 | a=GPIO : 0 réservé, 1 disponible/à nous, 2 détenu par l’autre application |
| 40 | a booléen : alimentation externe port A ; pas la recharge ni l’alimentation USB |

Exemple de consultation sans modification du matériel : `examples/hardware.c`.

```text
python tools/native-apps/build.py tools/native-apps/examples/hardware.c --toolchain CHEMIN/esp-rv32/2601/bin -o hardware.app.elf
```

Le binaire C partage la mémoire de l’OS, sans isolation matérielle. Un pointeur invalide ou une corruption mémoire peut donc faire tomber le système. Les erreurs Python ordinaires doivent remonter à la console ; une défaillance du pont C ou d’un pilote peut néanmoins réinitialiser le P4. L’absence de reset n’est pas une garantie fournie par ces API.
