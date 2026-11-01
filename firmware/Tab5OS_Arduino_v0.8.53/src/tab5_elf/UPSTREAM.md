# Espressif elf_loader 1.3.3 — Tab5 integration

Official component: https://components.espressif.com/components/espressif/elf_loader/versions/1.3.3

Archive download:
https://components.espressif.com/api/downloads/?object_id=e801c1d5-b6a5-420e-82fd-bd1c18b190c3&object_type=component

SHA-256 of downloaded archive:
`06b0862312b02c87b76dbcd0191038621a4f291c5c941b226576d7c12e875b7a`

License: Apache-2.0, copied in LICENSE. Only ELF core, ELF types, symbol interface
and RISC-V relocation code are vendored. Dynamic shared-object loading, exported
libc/OS tables, Xtensa and other-chip adapters are intentionally absent.

Local changes (not represented as unmodified upstream):

- Correct first PT_LOAD initialization (`!first_segment`).
- Correct physical relocation destination to `offset - svaddr`.
- Resolve symbol by RISC-V relocation semantics, not accidental STT_* constants;
  local symbols map relative to the segment; only the single registered Tab5
  API may be imported. Propagate every architecture relocation failure.
- R_RISCV_NONE returns before forming a relocation target pointer.
- Return the application's entry-point result.
- Pin log version to 1.3.3, limit registered symbol tables to one, use local
  relative includes compatible with Arduino recursive `src` compilation.
- Replace platform allocator with 64-byte-aligned PSRAM on ESP32-P4 only.
  Synchronize the admitted image's bounded range (data C2M, instruction M2C,
  local fence.i), after all relocations; remove global cache writeback and
  global interrupt/cache exclusion from the launch path.
- Keep Kconfig dynamic-object and bus-mirror features disabled (not defined).
  The bundled Arduino SDK is unchanged.

Before `esp_elf_relocate`, Tab5ElfValidate.cpp validates the entire file, including
all tables the upstream API assumes valid. The upstream API accepts a pointer
without a length, so this front-end is mandatory for every native app launch.

Compatibility verified with M5Stack Arduino 3.3.9 / ESP-IDF 5.5.4 / ESP32-P4.
Source review and host tests do not establish hardware runtime execution.
