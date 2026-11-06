# Hardware access - Tab5OS 0.8.9

The complete firmware, an `.app.elf` application and a MicroPython script do not have the same environment. The documented ABI can be extended using the supplied sources; an absent export must not be described as available.

| Feature | C `.app.elf` application | Supplied MicroPython | Complete C/C++ firmware |
|---|---|---|---|
| External GPIO, PWM, ADC | ABI 3 `hardware` callback | `machine.Pin`, `PWM`, `ADC` | Arduino / ESP-IDF |
| External I²C, SPI, serial | ABI 3 callback | `machine.I2C`, `SPI`, `UART` | SDK drivers |
| External port 5 V power | Operation 40 | `tab5.external_power()` | M5Unified |
| Display, buttons, audio | ABI 2/3, OS-managed surfaces and PCM | `tab5ui` bridge, existing APIs | OS drivers |
| Files | ABI 1 reading; ABI 2/3 game saves | `open`, `tab5fs` | Arduino FS / POSIX |
| Network through C6 | No general network API exported in this ABI | Existing Tab5OS network services | ESP-Hosted / Wi-Fi |
| Internal C6 GPIO, BLE, Thread, Zigbee | Not exported | Not exported | Requires suitable C6 firmware/protocol |
| GPIO interrupts, RMT, DMA or other raw drivers | Not exported | Not exported by this `machine` module | Requires ESP-IDF integration |

The C6 is a companion processor connected to the P4 through SDIO. Numbers passed to `machine.Pin` and the ABI identify **P4** pins, not C6 pins. Recompiling an application alone does not change the C6 firmware. Internal pins and buses used by the display, SD, touch, power or radio link remain reserved while the OS runs. An integration requiring these drivers must modify the complete firmware and manage their ownership; the application loader does not import arbitrary SDK symbols.

## Ownership and lifetime

A Python application and a C application can coexist. The `Tab5Hardware` broker reserves pins and buses for their user and returns `-EBUSY` if another application owns them. `deinit` or actual program return releases its resources. Stop requests are cooperative: they do not abruptly interrupt a hardware transaction. No callbacks or threads are retained after the application returns.

P4 pins offered by this version: **2, 3, 4, 5, 6, 7, 16, 17, 18, 19, 20, 21, 34, 35, 37, 38, 45, 47, 48, 51, 52, 53, 54**. Check the corresponding connector and physical mapping in the [M5Stack Tab5 pinout](https://docs.m5stack.com/en/core/Tab5). Some are shared with expansion/RS485 connector functions; software reservations do not detect external wiring.

ADC accepts only pins that actually have an analog channel according to the compiled P4 SDK. `Pin.deinit()`, `PWM.deinit()` and `ADC.deinit()` return the pin to input mode; for a pin owned by a bus, close the bus. A Pin object passed to a bus must be reserved by the same application.

## MicroPython

```python
from machine import I2C, UART, ADC, pin_available

print(pin_available(53))  # 0 reserved, 1 available/ours, 2 used by another app
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

UART: 8N1, non-blocking reads; `write` returns the number of bytes accepted, so handle partial writes, `any()` and `txdone()`. Baud rates range from 1,200 to 1,000,000. SPI: logical controller 2, 8-bit MSB-first, 1 kHz to 10 MHz, modes 0 to 3. I²C: logical ID 1, external hardware controller 0, 10 kHz to 1 MHz, 100 ms timeout. The OS internal I²C bus is not reset.

`ADC.read()` returns a 12-bit value, `read_u16()` scales it to 16 bits, and `read_uv()` uses the SDK millivolt conversion. The supplied classes are an explicit subset of the MicroPython API, not a promise of compatibility with all of its hardware libraries.

## ABI 3 for C

Import only `tab5_app_api_v3()` from `Tab5AppApi.h` and check `base.base.version == 3` and `base.base.size >= sizeof(*api)`. APIs 1/2 remain compatible with older applications. The callback is:

```c
int hardware(int op, int a, int b, int c,
             const void *input, size_t length,
             void *output, size_t capacity);
```

A negative result is `-errno`. Input/output is limited to 4,096 bytes per call; a pointer is required for nonzero lengths. All operations run on the application's worker.

| Operation | Parameters / result |
|---|---|
| 2 | a=GPIO, b=input 0/output 1/open-drain 2, c=no pull 0/up 1/down 2; reserves the pin |
| 3 / 4 | Read GPIO a / write GPIO a with boolean b |
| 5 | a=ADC GPIO, b=attenuation 0..3, c=0 raw value / c=1 millivolts; reserves on first call |
| 6 / 7 | PWM: a=GPIO, b=1..40,000 Hz, c=duty 0..65,535 / release PWM a |
| 9 | Release pin a (GPIO, PWM or ADC) |
| 10 | Open I²C: a=SDA, b=SCL, c=frequency |
| 11 | Write I²C: a=address 8..119, b=boolean STOP, input/length; returns bytes written |
| 12 | Read I²C: a=address, b=STOP, output/capacity <=128; returns bytes read |
| 13 | Close I²C |
| 20 | Open SPI: a=SCK, b=MOSI, c=MISO; CS managed separately through GPIO |
| 21 | Transfer SPI: a=frequency, b=mode; length==capacity; returns length |
| 22 | Close SPI |
| 30 | Open UART: a=TX, b=RX, c=baud rate |
| 31 / 32 | Write input/length (may be partial) / read output/capacity |
| 33 / 34 / 35 | Close UART / number of available bytes / TX complete (0 or 1) |
| 36 | a=GPIO: 0 reserved, 1 available/ours, 2 owned by the other application |
| 40 | Boolean a: external port A power; not charging or USB power |

Read-only query example that does not change hardware: `examples/hardware.c`.

```text
python tools/native-apps/build.py tools/native-apps/examples/hardware.c --toolchain PATH/esp-rv32/2601/bin -o hardware.app.elf
```

The C binary shares OS memory without hardware isolation. An invalid pointer or memory corruption can therefore crash the system. Ordinary Python errors should reach the console; a C bridge or driver failure can nevertheless reset the P4. These APIs do not guarantee that resets cannot occur.
