/* ESP32-S3 Xtensa LX7 dynarec emitter scaffolding.
 *
 * This backend intentionally starts with a 32-bit-safe writer. On ESP32-S3,
 * executable heap may be MALLOC_CAP_32BIT only, so generated code must not be
 * written byte-by-byte directly into the final cache.
 */

#ifndef XTENSA_EMIT_H
#define XTENSA_EMIT_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

typedef struct
{
  u8 *base;
  u8 *ptr;
  u8 *limit;
  u32 pending_word;
  u32 pending_bytes;
} xtensa_emit_buffer;

static xtensa_emit_buffer xtensa_emit;

static inline void xtensa_emit_bind(void *base, unsigned size)
{
  xtensa_emit.base = (u8 *)base;
  xtensa_emit.ptr = (u8 *)base;
  xtensa_emit.limit = (u8 *)base + size;
  xtensa_emit.pending_word = 0;
  xtensa_emit.pending_bytes = 0;
}

static inline void xtensa_emit_flush_word(void)
{
  if (xtensa_emit.pending_bytes == 0)
    return;

  *((volatile u32 *)xtensa_emit.ptr) = xtensa_emit.pending_word;
  xtensa_emit.ptr += 4;
  xtensa_emit.pending_word = 0;
  xtensa_emit.pending_bytes = 0;
}

static inline void xtensa_emit_u8(u8 value)
{
  xtensa_emit.pending_word |= ((u32)value << (xtensa_emit.pending_bytes * 8));
  xtensa_emit.pending_bytes++;

  if (xtensa_emit.pending_bytes == 4)
    xtensa_emit_flush_word();
}

static inline void xtensa_emit_u16(u16 value)
{
  xtensa_emit_u8((u8)value);
  xtensa_emit_u8((u8)(value >> 8));
}

static inline void xtensa_emit_u24(u32 value)
{
  xtensa_emit_u8((u8)value);
  xtensa_emit_u8((u8)(value >> 8));
  xtensa_emit_u8((u8)(value >> 16));
}

static inline u8 *xtensa_emit_tell(void)
{
  return xtensa_emit.ptr + xtensa_emit.pending_bytes;
}

static inline void xtensa_emit_finish(void)
{
  xtensa_emit_flush_word();
}

static inline void xtensa_patch_bytes(u8 *target, const u8 *src, unsigned count)
{
  for (unsigned i = 0; i < count; ++i)
  {
    uintptr_t addr = ((uintptr_t)target + i) & ~(uintptr_t)3;
    u32 shift = (((uintptr_t)target + i) & 3) * 8;
    volatile u32 *word = (volatile u32 *)addr;
    u32 value = *word;
    value = (value & ~(0xFFu << shift)) | ((u32)src[i] << shift);
    *word = value;
  }
}

/* Encodings verified with xtensa-esp32s3-elf-as/objdump. */
static inline void xtensa_emit_ret_n(void)
{
  xtensa_emit_u16(0xF00D);
}

static inline void xtensa_emit_retw_n(void)
{
  xtensa_emit_u16(0xF01D);
}

static inline void xtensa_emit_entry_a1_16(void)
{
  xtensa_emit_u24(0x002136);
}

static inline void xtensa_emit_movi_n_a2(u8 imm4)
{
  /* movi.n a2, imm4 */
  xtensa_emit_u16(0x020C | (((u16)imm4 & 0x0F) << 12));
}

void init_emitter(bool must_swap);
u32 execute_arm_translate(u32 cycles);

#endif
