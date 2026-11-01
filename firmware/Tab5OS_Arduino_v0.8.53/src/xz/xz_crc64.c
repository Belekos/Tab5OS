// SPDX-License-Identifier: 0BSD

/*
 * CRC64 using the polynomial from ECMA-182
 *
 * This file is similar to xz_crc32.c. See the comments there.
 *
 * Authors: Lasse Collin <lasse.collin@tukaani.org>
 *          Igor Pavlov <https://7-zip.org/>
 */

#include "xz_private.h"

/* Tab5: precomputed immutable data stays in flash, not scarce SRAM. */
#include "xz_crc64_table.h"

XZ_EXTERN void xz_crc64_init(void)
{
	/* Kept for source compatibility; the const table is already initialized. */
}

XZ_EXTERN uint64_t xz_crc64(const uint8_t *buf, size_t size, uint64_t crc)
{
	crc = ~crc;

	while (size != 0) {
		crc = xz_crc64_table[*buf++ ^ (crc & 0xFF)] ^ (crc >> 8);
		--size;
	}

	return ~crc;
}
