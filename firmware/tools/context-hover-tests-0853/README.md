# Context menu hover regression — 0.8.53

`main.cpp` injects relative mouse reports into `writing::usbMousePoll`. It executes the actual `Tab5Mouse::service`, `Tab5ContextMenu`, `Tab5Focus`, `Tab5UiStyle`, and LVGL 9.6. The test compares the delivered .52 source with current .53. Baseline success means that the reported defect was reproduced; current success means the rendered highlight and input transitions pass.

Run with a host LVGL build using ASan/UBSan and the shipped host configuration:

```sh
python3 tools/context-hover-tests-0853/run.py \
  --baseline ../baseline/Tab5OS_Ecriture_v0.8.52/Tab5OS_Arduino_v0.8.52_Ecriture \
  --lvgl ../build_0853/host-deps/lvgl-9.6.0 \
  --lvgl-build ../build_0853/host-lvgl \
  --build ../build_0853/context-hover
```

The runner also recompiles and executes the existing context-menu lifecycle/editing regression with current .53 production sources. It leaves the original historical test and its historical report unchanged.

Covered: actual rendered hover colour, preserved logical keyboard target, return to keyboard, disabled commands, deferred click, shortcut handling, native tactile input, replacement submenu, owner deletion, and idle input without refresh. The lifecycle regression additionally covers editing/history, protected fields, output binding, allocation refusal, and repeated menu creation/destruction.

USB transport, menu command effects and scene content are fixtures. This does not execute the complete Explorer page or its file operations, and makes no hardware or global memory-leak claim. LeakSanitizer is disabled for the host LVGL process; ASan and UBSan remain enabled. The lifecycle test separately checks its tracked text-history allocation counter.
