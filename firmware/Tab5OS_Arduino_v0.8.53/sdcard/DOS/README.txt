DOS for Tab5OS
=============

Open a 16-bit DOS .COM or MZ .EXE in Explorer, then run DOS. Its SD directory
and companion files appear on read-only drive C:. Long names receive DOS 8.3
aliases automatically. Start Applications/DOS.app.elf without an argument for
a FreeDOS prompt. Examples/HELLO.EXE reads Examples/DATA.TXT itself.

Use a USB keyboard or the Keys touch panel. Esc, F1-F12, arrows and Ctrl/Alt
are available. Keyboard: US / FR switches the FreeDOS layout at the prompt.
The French OS language selects French AZERTY automatically. The DOS canvas
toolbar remains English in this version.

A: is the bundled FreeDOS floppy. Its changes stay in RAM until Save or
Save & exit; they are saved separately in /Saves/DOS. The original FREEDOS.IMG
and the files behind C: are never modified by the emulator. Power loss or a
forced stop can lose unsaved changes. Check any reported save error.

Compatible target: 80286, 640 KiB conventional + 1 MiB extended RAM, VGA and
PC speaker. No Windows, 386/DPMI/DOS4GW, x87, AdLib or Sound Blaster support.
SD paths must be shorter than 256 UTF-8 bytes. Folder limits: 511 root entries,
1024 total entries, 16 directory levels. Choose a smaller folder if necessary.

This is real FreeDOS emulation. Physical Tab5 performance and peripherals
still require hardware validation. See firmware/DOS.md in the source tree
for full instructions, limitations and reproducible tests.

FreeDOS 1.4 kernel 2043, FreeCOM 0.86a, KEYB 2.11 and layouts 3.1a retain
their original licences. Matching source archives and notices are supplied
with the complete installation package under the DOS source/licence folder.
