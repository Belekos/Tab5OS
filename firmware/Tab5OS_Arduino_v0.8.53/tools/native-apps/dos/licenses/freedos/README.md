# FreeDOS boot image for Tab5OS

This is an unofficial, minimal image derived from the official FreeDOS 1.4
release. FreeDOS is a free DOS-compatible operating system; it is not Microsoft
MS-DOS. Project: https://www.freedos.org/

Included guest software:

- FreeDOS kernel 2043 (2021-05-14), the 8086-compatible KERNL86.SYS build,
  renamed KERNEL.SYS without modifying its bytes. Upstream: https://github.com/FDOS/kernel
- FreeCOM package 0.86a (2025-02-21), COMMAND.COM. Its embedded version text is
  "FreeCom version 0.86 - WATCOMC - XMS_Swap [Dec 30 2024 22:10:51]". Upstream: https://github.com/FDOS/freecom
- The FreeDOS FAT12 boot sector from 144m/x86BOOT.img, plus Tab5OS CONFIG.SYS
  and AUTOEXEC.BAT. No Microsoft binaries, games, installer, or memory drivers.

The kernel and FreeCOM packages identify their licence as GNU GPL version 2.
Their original licence texts and notices are preserved here and in the source
archives. FreeCOM's SUPPL library licence texts are also included. The source
archives are copied byte for byte from the same official release's packages;
the shipped kernel and shell are checked against those packages' binaries.

Keep this entire source-and-licence directory with any redistributed boot image.
kernel-source.zip and freecom-source.zip contain the corresponding source,
copyright notices and build scripts. For rebuilding, see docs/build.txt and
README.md in the kernel source, and docs/compile.txt plus build scripts in the
FreeCOM source. Required toolchains are described in those upstream documents.

Changes from the official release: the boot sector, kernel, shell and selected
keyboard tools are retained; the disk is reformatted as a 1.44 MB FAT12 floppy with fixed dates;
CONFIG.SYS and AUTOEXEC.BAT start an English command prompt without the installer.
Guest application compatibility is limited by the selected emulator CPU model.

Reproduce with Python 3 (standard library only):

    python prepare_freedos.py --cache-dir downloads --output freedos.img --licenses-dir freedos-sources --keyboard-tools

Add --offline to use previously downloaded, verified archives. Different existing
output images are preserved unless --force is explicitly supplied. Input release
URLs, SHA-256 digests, exact payload hashes and the generated image digest are
recorded in provenance.json. Official release checksums:
https://www.freedos.org/download/verify.txt

Optional keyboard tools are included in this build: KEYB 2.11 by Aitor Santamaria
Merino and keyboard layouts 3.1a by Henrique Peron, under GNU GPL version 2 as
their package metadata states (the KEYB README also permits later versions).
Matching sources, licence texts and help are included in this directory.
No keyboard driver is automatically loaded. At the DOS prompt, the command
`KEYB FR,437,A:\KEYBOARD.SYS` selects the standard French AZERTY layout for
hardware scancodes. `KEYB /U` unloads the driver and restores the BIOS layout.
The emulator's own text injection and touch keyboard must use the selected
guest layout; an emulator frontend may therefore keep its documented US layout.
Build this optional variant with `--keyboard-tools`.
