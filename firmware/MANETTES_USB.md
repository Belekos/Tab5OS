# USB gamepads

## 8BitDo SN30 Pro

The SN30 Pro has several modes, each exposing a different protocol. The Bluetooth model can also work over a cable. The model named **SN30 Pro USB**, with a fixed cable, is a different device.

| Model / mode | Connection for Tab5OS | Support in this version |
| --- | --- | --- |
| SN30 Pro Bluetooth, Android mode (`B` + `Start`) | Turn off the gamepad, hold `B` + `Start`, then connect a USB data cable to the Tab5's USB-A port | Generic HID driver, depending on the descriptor exposed by the gamepad |
| SN30 Pro Bluetooth, Windows mode (`X` + `Start`) | Turn off the gamepad, hold `X` + `Start`, then connect the cable | Added support for Xbox 360-style USB XInput transport |
| SN30 Pro USB, Windows mode | Connect its cable to USB-A | The same added XInput transport |
| Switch mode (`Y` + `Start` on the Bluetooth model) | - | Switch-specific initialization is not implemented; use one of the modes above |
| Wireless Bluetooth connection | - | No Bluetooth gamepad pairing in this version |

The official manual describes the startup combinations; it does not constitute a test of this gamepad on Tab5OS. Android mode is the mode to try with the HID driver. The added XInput path also reads wired `FF/5D/01` USB interfaces from 8BitDo and Microsoft. Xbox One protocols and wireless Xbox receivers are separate and are not included.

HID and XInput remain available simultaneously: Tab5OS chooses the decoder from the advertised USB interface, not solely from the device name or vendor ID. Settings do not impose an exclusive choice. Disconnect the gamepad before changing modes, then reconnect it with a data cable. 8BitDo documentation also calls HID mode D-input.

In **Settings → Interface → Gamepad**, you can see the connection and incoming controls without launching a game. **Configure gamepad** assigns the eight controls A, B, Select, Start, X, Y, L and R. Release the buttons between steps; **Cancel** or **Escape** exits without changing the configuration. Changing pages or tabs also cancels capture. Within a game, help (`F1` or the Help button) retains its display and configuration. The name **8BitDo SN30 Pro** is shown for the known ID `2dc8:6001`, and **8BitDo** for other IDs from this manufacturer. Active controls are displayed when pressed. **Configure gamepad** maps A, B, Select, Start and, in supported games, X, Y, L and R. The configuration is saved per model and protocol.

Vibration, motion sensors and manufacturer-specific features are not controlled. The physical Nintendo/Xbox button labels can vary with the mode: use configuration if A and B appear reversed.

## OS navigation and MicroPython

The gamepad is serviced from startup, without opening F1 help or an emulator. Outside a native game view:

| Configured control | Action |
| --- | --- |
| D-pad or left stick | Move focus; repeat while held |
| A / B | Confirm / Back |
| X or Select | Context menu |
| Y | Switch navigation area |
| Start | Help |
| L / R | Previous / next page |

Within an ELF game, controls remain assigned to the game; Guide still opens the F1 controls. Navigation is suspended while mapping buttons in Settings, so assigning B does not accidentally exit the wizard.

Standard `tab5ui` interfaces benefit from navigation without changes. A script can also read the state directly, independently of other readers:

```python
import gamepad
from time import sleep_ms

while True:
    state = gamepad.state()
    if state['connected'] and state['buttons'] & gamepad.A:
        print(state['axes'])
    sleep_ms(30)
```

`gamepad.buttons()` returns the current button mask (excluding keyboard input). `state()` also provides raw buttons, four signed axes X/Y/RX/RY, the available-axis mask, the protocol and statistics. Values range from -32768 to 32767; positive Y points down. Triggers are available as buttons depending on the protocol. `gamepad.capture(True)` disables navigation shortcuts only while the Python interface is in the foreground, for games that handle their own controls; `capture(False)` restores them. Capture is reset when each script starts. None of these calls consumes the events used by the OS or ELF applications.

## High-speed USB and hubs

**High-speed USB** remains a global USB host setting. It must be accessible even with no disk connected, particularly for RTL-SDR. Version 0.8.17 placed this option in the USB storage card, hidden when no disk was present; that was an incorrect location, not removal of the high-speed driver.

The choice is saved and takes effect after restarting. **Hub-compatible USB** remains the default. With the ESP-IDF stack in use, low/full-speed devices behind a high-speed hub are unsupported while the host remains in high-speed mode. To check an unrecognized gamepad, first connect it directly; with a hub, try compatible mode and restart.

USB speed does not add Bluetooth support and is not needed merely to decode a gamepad. Disk eject and mount buttons remain tied to the actual presence of a disk.
