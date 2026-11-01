# Explorer host preview

This fixture executes the .53 production Explorer constructors, row styles and image-action dispatch. The directory entries are demonstration data. It omits the outer OS shell and substitutes storage, USB transport and application-launch boundaries. No bitmap is retouched.

Adapt the three dependency paths to your host LVGL build:

```sh
python3 run.py --sketch /path/to/Tab5OS_Arduino_v0.8.53 --lvgl /path/to/lvgl-9.6.0 --lvgl-build /path/to/host-lvgl --build /path/to/output
```

`host-lvgl` must contain the matching `lv_conf.h` and `liblvgl.a`. The script writes only to the output directory and compiles with ASan/UBSan. Its 232 checks exercise this fixture; they are not new device tests.
