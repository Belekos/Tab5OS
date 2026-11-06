# Drawing engine

[`image_core.c`](image_core.c) and [`image_core.h`](image_core.h) provide the
Drawing raster engine. The firmware compiles this engine through
[`Tab5ImageCore.c`](../../../Tab5ImageCore.c); its integrated interface is in
[`Tab5ImageEditor.cpp`](../../../Tab5ImageEditor.cpp).

The [Drawing guide](../../../../DESSIN.md) describes the tools, formats and use
of the built-in application.

The `app.c` and `runtime.c` sources, together with their `build.py` recipe,
also build an ABI 6 ELF example using the same engine. From the sketch directory:

```bash
python3 tools/native-apps/image-editor/build.py --toolchain /path/esp-rv32/2601/bin -o sdcard/Applications/AtelierImage.app.elf
python3 tools/native-apps/image-editor/run_tests.py
```

The example has its own interface; the features of the built-in application
are documented in the Drawing guide.
