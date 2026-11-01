/*-
 * MIT License
 *
 * Copyright (c) 2019 kikuchan
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*----------------------------------------------------------------------------/
/ original source is here : https://github.com/kikuchan/pngle/
/
/ Modified for LGFX  by lovyan03, 2020-2022
/ tweak for 32bit processor
/----------------------------------------------------------------------------*/

#ifndef __TAB5_PNGLE_H__
#define __TAB5_PNGLE_H__

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Main Pngle object
typedef struct _tab5_pngle_t tab5_pngle_t;

// Callback signatures
typedef uint32_t (*tab5_pngle_read_callback_t)(void *user_data, uint8_t *buf, uint32_t len);
typedef void (*tab5_pngle_draw_callback_t)(void *user_data, uint32_t x, uint32_t y, uint_fast8_t div_x, size_t len, const uint8_t* argb);

// ----------------
// Basic interfaces
// ----------------
typedef void *(*tab5_pngle_alloc_callback_t)(void *, size_t);
typedef void (*tab5_pngle_free_callback_t)(void *, void *);
tab5_pngle_t *tab5_pngle_new();
/* Optional per-decoder allocator: all work buffers follow the caller ownership. */
tab5_pngle_t *tab5_pngle_new_with_allocator(tab5_pngle_alloc_callback_t,
    tab5_pngle_free_callback_t, void *);

int tab5_pngle_prepare(tab5_pngle_t *pngle, tab5_pngle_read_callback_t read_cb, void* user_data);
int tab5_pngle_decomp(tab5_pngle_t *pngle, tab5_pngle_draw_callback_t draw_cb);

void tab5_pngle_destroy(tab5_pngle_t *pngle);

uint32_t tab5_pngle_get_width(tab5_pngle_t *pngle);
uint32_t tab5_pngle_get_height(tab5_pngle_t *pngle);

// ----------------
// Debug interfaces
// ----------------

typedef struct __attribute__((packed)) _tab5_pngle_ihdr_t {
  uint32_t width;
  uint32_t height;
  uint8_t depth;
  uint8_t color_type;
  uint8_t compression;
  uint8_t filter;
  uint8_t interlace;
} tab5_pngle_ihdr_t;

// Get IHDR information
tab5_pngle_ihdr_t *tab5_pngle_get_ihdr(tab5_pngle_t *pngle);


#ifdef __cplusplus
}
#endif

#endif /* __PNGLE_H__ */
