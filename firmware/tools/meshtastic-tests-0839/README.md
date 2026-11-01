# Meshtastic 0.8.39 verification

`protocol.cpp` runs the production allocation-free codec against independent golden vectors, malformed protobuf, UTF-8 boundaries, output buffer canaries and 100,000 deterministic random packets.

`backend-ui.cpp` compiles the **unchanged namespace body** of `Tab5Meshtastic.cpp` with real LVGL and production Focus/TextInput/UiStyle. `backend-shim.h` replaces ESP/NimBLE boundary calls. It exercises actual callback bodies, security decisions, bounded tables, GATT fragment assembly, stale-connection protection, CCC selection, stable peer controls, keyboard layout and repeated session cleanup. It does not simulate radio propagation or run the FreeRTOS worker scheduler.

Run with the release's compatible LVGL archive:

```sh
python3 tools/meshtastic-tests-0839/run.py --lvgl /path/to/lvgl --lvgl-build /path/to/host-lvgl --build /tmp/tab5-meshtastic-tests
```

The LVGL build directory must contain `liblvgl.a` and its `lv_conf.h`. Tests use AddressSanitizer and UndefinedBehaviorSanitizer; LeakSanitizer is disabled because this executor does not expose the required process inspection. Explicit application allocation/free counts verify balance. The test script hashes tested production files and rejects a run if they change during execution. Physical C6/ESP-Hosted and Meshtastic radio validation remains required.
