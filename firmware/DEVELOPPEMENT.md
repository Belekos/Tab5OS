# Developing for Tab5OS

## Choose a format

| Need | Format |
| --- | --- |
| Form-based interfaces, scripts, small-file processing | MicroPython and `tab5ui` |
| Sustained computation or pixel rendering within a controlled memory budget | C compiled to ELF with `Tab5AppApi.h` |
| New driver or system service | Firmware changes and a full build |

The SDK is not a complete Linux or Arduino SDK. An ELF can import only the entry points `tab5_app_api_v1` through `tab5_app_api_v6`. A `.bin` file is firmware, not an application.

## A first Python project

Copy `examples/micropython/compteur` to `/Apps/compteur` on the card. The entry point is `main.py`. Open **Python Apps**, then refresh with **F5** if needed. The same script can be opened in the editor and run: its view is displayed directly.

The example uses native widgets, the active theme and keys that already have translations. It demonstrates a loop that waits 20 ms, reads events, closes cleanly in `finally`, and maps Back/previous/next to actions. Field-editing shortcuts remain those of the OS.

The `tab5ui` module exposes a logical surface of `1100 × 500` with at most 24 widgets per page. Use `ui.WIDTH` and `ui.HEIGHT` for placement, leave room for longer translations and use several views if the form exceeds this area. Do not copy physical screen coordinates, create a separate LVGL loop or reset the display.

Labels go through `ui.tr("Clé française")`: translation keys are French source strings. For a new key, update the language packs and check the glyphs; changing only the language of a Python comment does not translate the interface. Leave space between close, reset and delete buttons.

Python paths are logical and absolute: `/Documents/texte.txt` for microSD and `/USB/Documents/texte.txt` for USB storage. The `sd:/` and `usb:/` notations belong to launcher fields; do not pass them to `open()`.

Use `with open(...)` for files and `try/finally` for resources. Read in blocks; do not load an entire ebook, map or archive into a string. `tab5fs.ilistdir()` is an iterator that must be closed if iteration is interrupted. For downloads, `tab5fs.download(url, destination, limit=...)` uses the native HTTPS service; provide an appropriate limit. This module does not authorize a second independent network stack.

A script cannot assume that every module from standard ESP32 MicroPython is included. Available modules are those in the embedded port and `sdcard/MicroPython/lib`. See [MICROPYTHON_BIBLIOTHEQUES.md](MICROPYTHON_BIBLIOTHEQUES.md) for library installation.

## A first ELF

Open a terminal in `Tab5OS_Arduino_v0.8.53`. With the Espressif GCC toolchain installed (replace `PATH` with its location):

```text
python tools/native-apps/build.py tools/native-apps/examples/pattern.c --toolchain PATH/esp-rv32/2601/bin -o pattern.app.elf
```

A verified alternative for examples that do not depend on a C library:

```text
python tools/native-apps/build.py tools/native-apps/examples/pattern.c --compiler clang --toolchain PATH/esp-clang/bin -o pattern.app.elf
```

This alternative requires Python 3 and Espressif's `clang`, `ld.lld`, `llvm-readelf` and `llvm-strip` tools. It compiles RISC-V objects, then explicitly links them into a shared ELF. `--math` remains exclusive to GCC and its libraries; the Clang path does not provide an embedded libc.

Copy the ELF to `/Apps` or `/Applications` on microSD or USB. In the launcher, choose **No file** for `pattern.app.elf`. The other two modes request a file or remember a fixed file. `hash_file.c` is a file-reading example that receives its document in `argv[1]`; its FNV-1a hash is not cryptographic.

The project compiler rejects imports outside the API and preserves the previous output file if the new program fails to build or validate.

### Execution contract

The entry point is `int tab5_app_main(int argc, char **argv)`. Check `version` and `size` before calling an API. Supplied paths are `/...` or `/USB/...`. Do not retain any OS pointer after the application returns.

- Allocate with `api->base.allocate()` and free with `release()`; check every failure. The `pattern.c` program uses 24 KiB for its image.
- Check `should_stop()` regularly and call `sleep_ms()` to yield. Pixels and samples are copied before presentation callbacks return.
- Do not create tasks, persistent callbacks or an LVGL instance, or call Arduino/ESP-IDF symbols directly. A specialized native interface requires extending the firmware service and its contract.
- After cancellation, close files, free resources, then return. The stop button does not justify forcibly deleting a task that still owns files or memory.
- Heap limits are ceilings, not a promise of free memory. ABI 1: 1 MiB; ABI 2/3: 8 MiB; ABI 4: 20 MiB, subject to admission and resources already in use. Per-block and file limits are in `Tab5AppApi.h`.
- Current ABIs do not provide a generic network client. Do not bypass this limit with private imports. ABI 3 hardware access uses the shared service described in `tools/native-apps/HARDWARE.md`.

The ELF API does not automatically translate application-specific messages. The `language` field lets an application select its translations; graphical examples without text use the native view's shared controls. An OS dialog alone does not make a third-party application multilingual.

## Firmware organization

| Layer | Responsibility |
| --- | --- |
| Main sketch and `Tab5*` views | Navigation, LVGL, theme, interaction |
| `Tab5Task`, `Tab5Memory`, `Tab5ResourceManager` | Tasks, budgets and resource state |
| `Tab5NetworkGate` and network services | Connection admission and native operations |
| Shared storage and USB host | Volume access, device sharing |
| MicroPython / ELF loader | Cooperative application execution and cleanup |

Do not call LVGL from a network or storage worker. Produce a bounded result and let the UI task display it. When processing ends, C++ destructors must run before `vTaskDelete()`; that function does not unwind the C++ stack.

## Verify a change

Rebuild using the [build guide](COMPILATION.md). After changing SD resources, regenerate the affected files using the documented scripts.

For a hardware test, record the version, peripherals, open application, memory before/after and exact sequence. Repeat open/close cycles and cancellations, then remove storage at a point supported by the interface. A successful build alone validates neither hardware stability nor peak SRAM requirements.

## ELF ABI 6 — canvas and images

`tab5_app_api_v6` retains the ABI 5 prefix. `canvas_begin` creates a canvas up to 800 × 480, without game controls. `input_poll` copies touch and keyboard events; CANCEL asks the application to cancel the current gesture after event loss or overflow, while CLOSE asks it to handle closing. `write_begin`, `write_chunk` (at most 4,096 bytes) and `write_finish` expose progressive writing restricted to `/Images/<name>.bmp` or `.t5i`, with a 16 MiB limit and a single stream. Closing the application abandons an uncommitted stream. Exact definitions are in `Tab5AppApi.h`. The current editor uses `Tab5ImageEditor.cpp` in the firmware, with LVGL and the shared C engine.
