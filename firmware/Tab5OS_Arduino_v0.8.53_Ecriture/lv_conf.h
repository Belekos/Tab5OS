#ifndef LV_CONF_H
#define LV_CONF_H

/* Tab5OS Arduino - LVGL 9.6.0 configuration.
   Place this file NEXT TO the lvgl folder, for example:

   Documents/Arduino/libraries/
      lvgl/
      lv_conf.h
*/

#define LV_COLOR_DEPTH 16
#define LV_DPI_DEF 180
#define LV_DEF_REFR_PERIOD 16
#define LV_TXT_ENC LV_TXT_ENC_UTF8

/* Localised labels may mix Arabic and Latin text. Detect each paragraph's
   direction and shape Arabic/Persian letters before TinyTTF renders them.
   The supplied SD font contains the required presentation-form glyphs. */
#define LV_USE_BIDI 1
#define LV_BIDI_BASE_DIR_DEF LV_BASE_DIR_AUTO
#define LV_USE_ARABIC_PERSIAN_CHARS 1

/* Do not wrap French compound words after '-'. LVGL includes '-' in its
   default break characters, which changes both visual wrapping and EPUB
   pagination. Keep normal punctuation/space breaks but remove hyphen. */
#define LV_TXT_BREAK_CHARS " ,.;:_)]}"
#define LV_TXT_LINE_BREAK_LONG_LEN 0

/* Native LVGL TLSF over elastic PSRAM segments. The adapter returns empty
   segments to ESP-IDF; no fixed multi-megabyte pool is reserved at boot.
   LV_MEM_SIZE configures TLSF's supported block range here, not a reservation. */
#define LV_USE_STDLIB_MALLOC LV_STDLIB_CUSTOM
#define LV_USE_TLSF 1
#define LV_MEM_SIZE (32U * 1024U * 1024U)

/* Long text optimisation for editor / preview. */
#define LV_LABEL_LONG_TXT_HINT 1

/* Bitmap fallbacks. They mainly provide LVGL/FontAwesome symbols if the TTF
   does not contain the private-use glyphs used by LV_SYMBOL_*. */
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_22 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_MONTSERRAT_32 1
#define LV_FONT_DEFAULT &lv_font_montserrat_20

/* Smooth UTF-8 text. /System/font.ttf is loaded from SD into PSRAM. */
#define LV_USE_TINY_TTF 1
#define LV_TINY_TTF_FILE_SUPPORT 0

/* Rich EPUB renderer. */
#define LV_USE_SPAN 1
#define LV_USE_TABLE 1
#define LV_USE_FLEX 1

/* No coloured default theme. The application uses the monochrome theme only
   as a neutral base and explicitly styles its widgets in greys. */
#define LV_USE_THEME_DEFAULT 0
#define LV_USE_THEME_SIMPLE 0
#define LV_USE_THEME_MONO 1

#define LV_USE_LOG 0

#endif /* LV_CONF_H */
