# Tab5OS Game Boy Color application

Freestanding Tab5 application using the gnuboy core from retro-go. The complete
application is distributed under GPL version 2; see core/COPYING and
core/CREDITS. Sources and build helper are included with the binary. Original
upstream provenance and modifications are recorded in core/UPSTREAM.txt.

Build from the sketch root:

```
python tools/native-apps/gameboy-color/build.py --toolchain PATH/esp-rv32/2601/bin -o sdcard/Applications/gameboy_color.app.elf
```

Requires the Tab5 ABI 2 application SDK and the Espressif RISC-V compiler.
Tab5OS 0.8.9 adds the /Saves/GBC save location used by this application.
The firmware is built separately. This source directory is not linked into
the firmware's permanent application set.

Supported: GB/CGB, complete 32 KiB–4 MiB ROMs, standard MBC0/1/2/3/5,
160×144 RGB565 and 22050 Hz stereo. No GBA, cable link or cartridge sensors.
Saved-state format is specific to this port; SRAM saves are separate.
The RTC does not advance while the application is stopped.

The included Tab5_Color.gbc demonstration is original homebrew and dedicated
to the public domain under CC0-1.0. Its make_demo.py generator uses the shipped
DejaVu font (its licence is supplied separately). No commercial game is bundled.
See [the application guide](../../../../APPLICATIONS.md) for installation and the supplied programs.

Host tests exercise rendering, state resume/corruption, write failures and tracked
allocation cleanup. Hardware framerate and game compatibility are unmeasured.
