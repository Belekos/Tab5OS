# DOS on Tab5OS

DOS runs a real PC/AT with an Intel 80286 interpreter, a built-in BIOS and
FreeDOS 1.4. It is a native application on the SD card, separate from the OS.
The complete candidate installation archive includes the application, boot
image, examples, licences and corresponding FreeDOS source code.

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

Use a USB keyboard or the **Keys** touch panel. The touch panel includes
Escape, F1â€“F12, arrows, Tab, Backspace, Delete, Enter and sticky Ctrl/Alt.
Physical key presses and releases are delivered as PC scan codes, including
held keys. The DOS toolbar uses English labels in this version; the surrounding
Explorer and OS controls use the selected Tab5OS language.

The guest starts with a US keyboard. When the OS language is French, the
frontend loads the supplied FreeDOS French AZERTY driver. The toolbar's
**Keyboard: US / FR** button switches layouts at the DOS prompt. Change layouts
before starting a program. This button types the corresponding FreeDOS command;
it cannot change a running game's keyboard driver safely.

Manual commands are `KEYB FR,437,A:\KEYBOARD.SYS` for French and `KEYB /U`
to restore the BIOS US layout. The matching driver and layout sources are
included with the boot image's licences.

## Files and saved changes

`C:` is a **read-only projection** of the selected SD folder. Programs can read
their companion files but cannot modify the originals. `A:` is the bundled
FreeDOS boot floppy. Guest writes to `A:` use a bounded sector overlay in RAM;
the original `/DOS/FREEDOS.IMG` is always opened read-only.

**Save** commits the changed floppy sectors to `/Saves/DOS/<identity>.state`.
**Save & exit**, and the OS's normal close request, also save before returning.
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
1 MiB extended memory, VGA/CGA/EGA-compatible video and a PC speaker. Graphics
are scaled into the application canvas. Actual emulation speed depends on the
program and must still be assessed on the physical Tab5.

Use 16-bit DOS COM and MZ EXE programs. Windows PE/NE/LE/LX executables are
rejected. 386 instructions, DOS/4GW and other 386/DPMI extenders, Windows,
an x87 coprocessor, AdLib and Sound Blaster are outside this version's scope.
Arbitrary disk-image mounting, host drive writes and networking are not exposed.

Host tests boot the real FreeDOS kernel, run original COM/MZ programs, resolve
long names in directories containing spaces, and read an adjacent data file.
Firmware and native ELF validation provide additional checks; these tests do
not replace a final hardware test of timing, USB input, display and audio.

## Sources and licences

The emulator derives from the MIT-licensed `rlucencic/esp32pc` and
`rlucencic/esp32emu` projects. The IBM-compatible bitmap font reproductions are
CC BY-SA 4.0. FreeDOS components retain their original GPL licences and matching
source archives. See [`UPSTREAM.txt`](tools/native-apps/dos/UPSTREAM.txt) and
[`FreeDOS provenance`](tools/native-apps/dos/licenses/freedos/provenance.json).
