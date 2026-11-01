Source: https://github.com/richgel999/jpeg-compressor
Commit: aeb7d3b463aa8228b87a28013c15ee50a7e6fcf3
License: public domain (upstream dual public-domain / Apache-2.0).
Only the scanline encoder is used.
Tab5 adaptation: the single internal scanline allocation explicitly uses PSRAM on Arduino; host builds use malloc. Line endings normalized to LF.
