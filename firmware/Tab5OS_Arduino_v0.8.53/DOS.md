# DOS on Tab5OS

DOS runs a real PC/AT with an Intel 80286 interpreter, a built-in BIOS and
FreeDOS 1.4. It is a native application on the SD card, separate from the OS.
The complete candidate installation archive includes the application, boot
image, examples, licences and corresponding FreeDOS source code.

The SD card's `/DOS/README.txt` is retained unchanged to preserve existing DOS
snapshots. It describes the original candidate; its statement that AdLib and
Sound Blaster are unavailable no longer applies. This guide describes the
current controls and capabilities.

## Start a program

Open a `.COM` or `.EXE` file in Explorer, or choose **Open with > DOS**.
The selected file's directory becomes drive `C:`. Its neighbouring files and
subdirectories are available to the program. Names longer than DOS's 8.3 format
receive ASCII short aliases; accented characters and shell punctuation are
replaced safely. The launcher resolves the selected program's actual
alias automatically. Opening `/Applications/DOS.app.elf` without an argument
starts the FreeDOS prompt with `/DOS` mounted as `C:`.

Try `/DOS/Examples/HELLO.COM` or `/DOS/Examples/HELLO.EXE`. The latter opens
the adjacent `DATA.TXT` itself. Their source generator is
[`make_examples.py`](tools/native-apps/dos/tests/make_examples.py).

The first version supports SD paths shorter than 256 UTF-8 bytes, up to 511
entries in the selected directory, 1,024 entries in its complete directory
tree and 16 nested directory levels. A directory that exceeds these limits
fails to mount with a diagnostic; choose a smaller program directory.

## Controls

Use a USB keyboard or the **Keyboard** touch panel (**Keys** in text mode).
The touch panel includes
Escape, F1-F12, arrows, Tab, Backspace, Delete, Enter and sticky Ctrl/Alt.
Physical key presses and releases are delivered as PC scan codes, including
held keys. The DOS toolbar uses English labels in this version; the surrounding
Explorer and OS controls use the selected Tab5OS language.

Graphics use an enlarged 4:3 picture, including 320 x 200 VGA games whose
original monitor used non-square pixels. The controls are grouped to its right;
Escape and F5 remain directly accessible. F5 is sent to the game and only opens
a menu when that game assigns it that function. Text mode retains its unscaled
80-column glyphs with the controls below. Opening the touch keyboard temporarily
reduces the picture above the keys. Use the OS's **Quit** button to save the DOS
disk overlay and leave; there is no second quit button inside the DOS canvas.

The guest starts with a US keyboard. When the OS language is French, the
frontend loads the supplied FreeDOS French AZERTY driver. The toolbar's
**US keys / FR keys** button switches layouts at the DOS prompt. Change layouts
before starting a program. This button types the corresponding FreeDOS command;
it cannot change a running game's keyboard driver safely.

Manual commands are `KEYB FR,437,A:\KEYBOARD.SYS` for French and `KEYB /U`
to restore the BIOS US layout. The matching driver and layout sources are
included with the boot image's licences.

## Mouse and touchpad

A USB mouse moves the DOS cursor and supplies left/right buttons, including
hover, held buttons and dragging. The emulated mouse is a Microsoft serial
mouse on COM1/IRQ4. The real CuteMouse 2.1b4 driver is loaded automatically from
the read-only driver disk `B:` with `B:\CTMOUSE /S14 /R11`; its normal INT33
services and application callbacks are available to DOS programs. Wheel and
middle-button actions are not exposed by this two-button guest device.

For games, tap the OS toolbar's **Mouse** button (**Souris** in French) to capture the USB mouse. Its
status changes from **Free** to **Captured**, the desktop pointer disappears, and movement can continue
without desktop screen-edge limits. Tap that same button on the touchscreen
to release capture; the touchscreen remains available throughout. Leaving DOS
or disconnecting the mouse also releases capture. No guest keyboard shortcut
is intercepted. Without capture, the mouse controls DOS while over its canvas.
Capture changes take effect after the current input event has returned. This
avoids reading LVGL's mouse input recursively from the capture button's click.

The game picture acts as a touchpad: slide a finger to move the
cursor, tap for a left click, hold still for half a second for a right click,
or tap and hold the second touch to drag. Lifting the finger releases a held
button. The on-screen keyboard keeps its existing key controls. Changing the
display layout, leaving the application or losing queued input releases buttons.

Install the matching firmware and SD files together for mouse support. Only
DOS opts into the extended canvas mouse events; other applications retain their
existing pointer behaviour. The auxiliary `/System/DOS/DRIVERS.IMG` contains the
driver, matching source and licence texts. It does not replace `FREEDOS.IMG` or
change the original `/DOS` folder, so existing saved floppy overlays retain
their identity.

## Sound and volume

PC speaker, AdLib OPL2 music and Sound Blaster 2.0 8-bit PCM sound are available.
For games with a sound setup program, select AdLib/OPL2 for music, or Sound
Blaster 2.0 with base port `220`, interrupt `7` and DMA channel `1`. The frontend sets
`BLASTER=A220 I7 D1 T3` before launching the program. MIDI/MT-32, CD audio and
Sound Blaster Pro/16, ADPCM playback and recording are not provided by this application.

Use **-** and **+** beside the volume value to adjust the application volume from
0 to 100%. Tap the middle volume value to mute and tap it again to restore the
previous level. This gain is also subject to the OS's master volume. The guest's
sound hardware continues advancing while muted. A busy or failed host audio
output is reported; adjusting volume retries output without restarting DOS.

Guest samples are submitted at their original 22,050 Hz rate, preserving pitch.
The adaptive interpolation introduced in candidate 31bc5c7 was removed after
hardware reports of distorted sound and reduced responsiveness. A slow audio
write no longer causes extra audio to be generated from elapsed wall time.
Delayed output is bounded, and muting stops host writes after completing the
last partial block. Guest sound timers and DMA continue advancing. A guest
that cannot keep up can still produce gaps; this does not guarantee full-speed
audio or game playback on the Tab5.

## Files and saved changes

`C:` is a **read-only projection** of the selected SD folder. Programs can read
their companion files but cannot modify the originals. `A:` is the bundled
FreeDOS boot floppy. Guest writes to `A:` use a bounded sector overlay in RAM;
the original `/DOS/FREEDOS.IMG` is always opened read-only.

**Save** commits the changed floppy sectors to `/Saves/DOS/<identity>.state`.
The OS's normal close request also saves before returning.
A snapshot is tied to the program directory, its layout and the boot image.
The OS atomically replaces the snapshot, so a failed save retains the previous
committed file. Forced termination or power loss can lose changes made since
the last successful save. Save errors are reported; the source files and base
image remain unchanged.

The overlay has room for the complete 1.44 MiB floppy and is serialized without
allocating a second full copy. To run a program that must create files, copy it
and its small data files to `A:` from the DOS prompt first, within the floppy's
available capacity. `C:` remains read-only.

## Compatibility and limits

The machine exposes an 80286 at a nominal 6 MHz, 640 KiB conventional memory,
1 MiB extended memory and VGA/CGA/EGA-compatible video. Graphics are scaled into
the application canvas. The renderer now avoids redundant VGA snapshots,
unchanged screen submissions and per-pixel scaling divisions. The runtime and
speaker timing also avoid costly general-purpose 64-bit divisions where their
inputs fit the P4's native arithmetic.

Hide the **Keys** panel to see the bottom status. It shows **PC %**, the speed of the emulated machine relative to
its nominal timing, and **fps**, changed images submitted to the OS. PC 100%
means the guest keeps up with its configured timing; it does not mean the P4's
CPU is fully occupied. A stationary DOS prompt naturally has a low image rate.
These indicators make a physical game test more useful than an image count
alone. The console also records guest CPU, video and audio processing time.

The DOS application runs on P4 core 0. The OS handles the display separately;
the second core is not simply unused. Within DOS, the x86 interpreter, image
preparation and AdLib synthesis still execute sequentially. The application
reserves an 8 MiB heap arena in PSRAM; the guest sees 640 KiB conventional RAM
and 1 MiB extended RAM. The interpreter code, guest memory and 48 KiB task stack
also use PSRAM. Memory access and cache costs may affect speed, but a low PC
percentage does not by itself indicate insufficient memory capacity.

The **Info** panel reports elapsed CPU, audio and video work times over the
displayed interval, plus free bytes and the largest free block in the DOS heap.
These are wall-clock durations, including scheduling delays, not utilization
measurements for either P4 core. Heap figures describe the application's own
8 MiB arena, not all free physical PSRAM or the guest's conventional memory.

The current optimization skips raster-history calculations and snapshot work
for frames that will not be displayed, while retaining the next frame's live
register state. AdLib noise generation uses exact lookup transitions instead
of repeated single-bit steps. All 8,388,608 valid noise states are checked
against the original recurrence, and the reference PCM remains unchanged.
A nine-voice PC benchmark measured about 24% less synthesis time; this is not
a Tab5 speed measurement and does not predict a corresponding game FPS gain.

Candidate 3b5df2d was reported at PC 56% in Monkey Island with 5-6 screen
submissions per second. Candidate 31bc5c7 was then reported at 1 fps with
distorted sound. This correction removes its adaptive audio path and repairs
graphics change detection for the final VGA palette entry. The fps counter
counts changed pictures submitted to the display, so it must be considered
alongside PC speed and visible animation. Host tests verify PCM identity,
bounded work and changing pixels; they do not establish a new hardware frame
rate or guarantee that every game is playable.

The subsequent physical report for candidate 2b3c7c3 confirmed working radio,
restored sound pitch and improved DOS image rate, but remaining sound gaps
and input latency in Monkey Island. This update retains that fixed-rate audio
path. Hardware measurements from the Info panel are needed to distinguish
interpreter, sound and image costs; uninterrupted audio is not guaranteed.

Use 16-bit DOS COM and MZ EXE programs. Windows PE/NE/LE/LX executables are
rejected. 386 instructions, DOS/4GW and other 386/DPMI extenders, Windows and
an x87 coprocessor remain outside this version's scope. 386/486 compatibility
requires a different CPU core; increasing a MHz setting cannot add it.
Arbitrary disk-image mounting, host drive writes and networking are not exposed.

Host tests boot the real FreeDOS kernel, run original COM/MZ programs, resolve
long names in directories containing spaces, and read an adjacent data file.
Firmware and native ELF validation provide additional checks; these tests do
not replace a final hardware test of timing, USB input, display and audio.

The original `/DOS` contents and `FREEDOS.IMG` remain unchanged in this update.
The new driver disk is outside that directory so existing disk-overlay identities
remain valid when the program directory itself has not changed.

The display/performance update after candidate 2b3c7c3 retains the same ABI7.
If that firmware is already installed, replacing `/Applications/DOS.app.elf`
with the new packaged file is sufficient; reflashing is not required.

## Sources and licences

The emulator derives from the MIT-licensed `rlucencic/esp32pc` and
`rlucencic/esp32emu` projects. The IBM-compatible bitmap font reproductions are
CC BY-SA 4.0. FreeDOS components retain their original GPL licences and matching
source archives. See [`UPSTREAM.txt`](tools/native-apps/dos/UPSTREAM.txt) and
[`FreeDOS provenance`](tools/native-apps/dos/licenses/freedos/provenance.json).
