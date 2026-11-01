# OpenWith tests

Run `run.py` with `--build`, `--lvgl` and `--lvgl-build` paths. `--draft` runs the checks without publishing a source-frozen report.

The suite builds all 132 production MicroPython C sources and executes scripts against its real `sys.argv` implementation. Native OS calls are test doubles. It verifies Unicode/punctuation delivery, the existing no-document entry point, and argument isolation across VM restarts.

The LVGL harness extracts the actual OpenWith construction/actions, Python chooser and switch handlers, and the pending-switch service branch from `Tab5Tools.cpp`. UI, focus and text controls use real LVGL and the shipped font. The surrounding Tools shell, storage, picker, and application callbacks are fixtures. Six native routes in the sketch are also checked statically.

ASan/UBSan cover the UI/routing/path harness; the conservative MicroPython GC is tested without ASan. No physical Tab5, USB transport, ESP32 task, or application parser is executed by this suite.
