#pragma once
/* Tab5 native application ABI 1. Only import: tab5_app_api_v1.
 * Trusted code shares OS memory. This interface is NOT a security sandbox.
 * Entry point: int tab5_app_main(int argc, char **argv), argc == 1 or 2:
 * argv[0] = application path; argv[1] = optional nonempty user argument.
 * argv[argc] is NULL. Check argc before reading the optional argument.
 * No threads, retained callbacks, C++ constructors, libc, or OS symbol imports.
 * All callbacks must execute on the application's worker. Return to end.
 */
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define TAB5_APP_ABI_V1 1u
#define TAB5_APP_MAX_ALLOCATION (256u*1024u)
#define TAB5_APP_MAX_HEAP (1024u*1024u)
#define TAB5_APP_MAX_IO 4096u
#define TAB5_APP_MAX_FILES 8u
struct tab5_app_api {
  uint32_t version;
  uint32_t size;
  const char* language; /* "en" or "fr", fixed until the entry point returns */
  void (*log)(const char* bytes,size_t length); /* at most 4096 bytes/call */
  int (*should_stop)(void);
  int (*sleep_ms)(uint32_t milliseconds); /* yields, returns 1 on cancellation */
  void* (*allocate)(size_t bytes); /* tracked PSRAM, 32 blocks; ABI 1: 1 MiB total; ABI 2/3: 8 MiB total; ABI 4: 20 MiB total, 16 MiB/block. Available during cooperative cleanup. */
  void (*release)(void* address);
  int (*open_read)(const char* sd_path); /* /... canonical SD path; handle > 0 */
  int (*read)(int handle,void* destination,size_t capacity); /* bytes, 0 EOF, -1 error */
  int (*seek)(int handle,uint32_t absolute_offset); /* 0 success, -1 error */
  int (*close)(int handle); /* 0 success, -1 error */
  uint32_t (*millis)(void);
};
extern const struct tab5_app_api* tab5_app_api_v1(void);
/* ABI 2 adds bounded graphics, held input, PCM and atomic game saves.
 * ABI 1 remains binary compatible. Graphics are presented by the OS UI task.
 * Buttons: bits 0..7 = Right Left Up Down A B Select Start; bit 8 = Exit.
 * Bits 9 (save state) and 10 (restart) are one-shot requests, consumed on read.
 * All pixel/sample buffers are copied before callbacks return. */
#define TAB5_APP_ABI_V2 2u
struct tab5_app_api2 {
  struct tab5_app_api base;
  uint32_t (*file_size)(int handle);
  int (*video_begin)(uint16_t width,uint16_t height);
  int (*video_present)(const uint16_t* rgb565,size_t pixels);
  uint32_t (*buttons)(void);
  int (*audio_pcm)(const int16_t* stereo,size_t frames,uint32_t sample_rate);
  int (*save)(const char* path,const void* data,size_t bytes); /* allowlisted /Saves/<system>: *.sav / *.state, <=512KiB; ABI 4: also *.cfg / *.dsg, <=2MiB */
};
extern const struct tab5_app_api2* tab5_app_api_v2(void);
/* ABI 3 preserves the ABI-2 prefix and exposes the same peripheral broker as
 * MicroPython. See tools/native-apps/HARDWARE.md for operation numbers.
 * OS 0.8.25 adds environment broker 107/108/109; see Tab5Environment.h.
 * No new ABI import required; secret reads require the unlocked vault. */
#define TAB5_APP_ABI_V3 3u
struct tab5_app_api3 {
  struct tab5_app_api2 base;
  int (*hardware)(int operation,int a,int b,int c,const void* input,size_t length,void* output,size_t capacity);
};
extern const struct tab5_app_api3* tab5_app_api_v3(void);
/* ABI 4 keeps the ABI-3 prefix. Extended controls: X/Y/L/R at bits 11..14.
 * Requests remain bits 8..10. Large apps request PSRAM through the same OS
 * cache/admission manager (up to 20 MiB total, 16 MiB per allocation).
 */
#define TAB5_APP_ABI_V4 4u
struct tab5_app_api4 {struct tab5_app_api3 base;uint32_t (*unix_time)(void);uint64_t (*floating)(unsigned operation,uint64_t a,uint64_t b);int (*calendar)(uint32_t seconds,int utc,int fields[9]);};
extern const struct tab5_app_api4* tab5_app_api_v4(void);
/* ABI 5 keeps every previous prefix. An optional generic form/console is
 * rendered on the OS UI task, with its theme, fonts and editing shortcuts.
 * Strings passed to panel_open are copied; no ELF pointer is retained.
 * Modes are newline-separated untranslated OS dictionary keys (1..8 modes).
 * value_modes selects which modes expose the secondary field (bit per mode).
 */
#define TAB5_APP_ABI_V5 5u
#define TAB5_PANEL_RUN 1u
#define TAB5_PANEL_CLEAR 2u
#define TAB5_PANEL_CLOSE 3u
#define TAB5_PANEL_READY 0u
#define TAB5_PANEL_RUNNING 1u
#define TAB5_PANEL_DONE 2u
#define TAB5_PANEL_ERROR 3u
#define TAB5_PANEL_STOPPED 4u
struct tab5_app_panel_spec {
  uint32_t size;
  const char *title,*modes,*target_label,*target_hint,*value_label,*value,*run_label;
  uint32_t value_modes;
  const char *initial_target;
  uint32_t alternate_modes;
  const char *alternate_hint;
};
struct tab5_app_panel_event {
  uint32_t size,action,mode;
  char target[2049],value[513];
};
struct tab5_app_api5 {
  struct tab5_app_api4 base;
  int (*panel_open)(const struct tab5_app_panel_spec* spec);
  /* Nonblocking: 1 event, 0 none, -1 closed/error. RUN fills all fields;
   * CLEAR/CLOSE only change action, preserving the active command arguments. */
  int (*panel_poll)(struct tab5_app_panel_event* event);
  int (*panel_write)(const char* bytes,size_t length,int reset); /* <=4096/call */
  int (*panel_state)(uint32_t state);
  /* Only dns, ping, ports and headers. argv[0] names the operation, <=8 args,
   * <=4096 argument bytes total. No shell parsing, child task or saved callback.
   * Output runs synchronously on this ELF worker. OS owns connections/leases.
   * Stop in the panel cancels this operation (130), leaving the app open. */
  int (*network)(int argc,const char* const* argv,
                 void (*output)(void* user,const char* bytes,size_t length),void* user);
  /* Copies a translation into capacity bytes; returns full byte count, excluding
   * NUL. Missing keys return the key. Works with every installed OS language. */
  size_t (*translate)(const char* key,char* output,size_t capacity);
};
extern const struct tab5_app_api5* tab5_app_api_v5(void);
/* ABI 6: pointer canvas and transactional image files. The OS copies input and
 * frames; ELF code never runs on the UI task. Canvas <=800x480; heap limits of
 * ABI 4 still apply. CLOSE requests app confirmation; Escape is a KEY event. */
#define TAB5_APP_ABI_V6 6u
#define TAB5_INPUT_DOWN 1u
#define TAB5_INPUT_MOVE 2u
#define TAB5_INPUT_UP 3u
#define TAB5_INPUT_KEY 4u
#define TAB5_INPUT_CANCEL 5u
#define TAB5_INPUT_CLOSE 6u
/* Optional mouse reports preserve the ABI-6 event layout. POINTER_POSITION
 * gives absolute canvas coordinates (possibly outside); POINTER_RELATIVE gives
 * raw HID motion before desktop edge clamping. They arrive as one ordered pair.
 * key is the held logical button mask: primary=1, secondary=2, middle=4.
 * Older apps may ignore these types and retain legacy primary-click events. */
#define TAB5_INPUT_POINTER_POSITION 7u
#define TAB5_INPUT_POINTER_RELATIVE 8u
#define TAB5_POINTER_MOUSE 0x80000000u
/* OR this opt-in flag into canvas_begin's width. The remaining 15 bits are
 * the real width (1..800). The app receives rich mouse pairs instead of duplicate
 * legacy USB pointer events; touch DOWN/MOVE/UP is unchanged. Older firmware
 * rejects the flagged dimensions instead of silently delivering incomplete input. */
#define TAB5_CANVAS_MOUSE_EVENTS 0x8000u
#define TAB5_KEY_TEXT 0u
#define TAB5_KEY_ENTER 1u
#define TAB5_KEY_ESCAPE 2u
#define TAB5_KEY_BACKSPACE 3u
#define TAB5_KEY_DELETE 4u
#define TAB5_KEY_LEFT 5u
#define TAB5_KEY_RIGHT 6u
#define TAB5_KEY_UP 7u
#define TAB5_KEY_DOWN 8u
#define TAB5_KEY_TAB 9u
#define TAB5_MOD_CTRL 1u
#define TAB5_MOD_SHIFT 2u
#define TAB5_MOD_ALT 4u
#define TAB5_MOD_META 8u
struct tab5_app_input_event {
 uint32_t size,type; int32_t x,y; uint32_t key,modifiers; char text[12];
};
struct tab5_app_api6 {
 struct tab5_app_api5 base;
 int (*canvas_begin)(uint16_t width,uint16_t height);
 /* Fill event.size before calling. 1 event, 0 empty, negative error.
  * CANCEL resets an unfinished gesture after lost input/queue overflow. */
 int (*input_poll)(struct tab5_app_input_event* event);
 /* One stream, /Images/<name>.bmp or .t5i, <=16MiB. write_begin returns a
  * positive handle; chunk returns bytes written (<=4096), negative on error.
  * finish(handle,1) commits; finish(handle,0) aborts. Failure retains previous bytes; an interrupted rename is recovered
  * on the next image open/save. Cancellation/worker cleanup aborts pending output. No whole-file copy. */
 int (*write_begin)(const char* path);
 int (*write_chunk)(int handle,const void* data,size_t bytes);
 int (*write_finish)(int handle,int commit);
};
extern const struct tab5_app_api6* tab5_app_api_v6(void);
/* ABI 7: non-consuming physical keyboard state and bounded read-only directory
 * enumeration. Existing ABI layouts and canvas text events are unchanged. */
#define TAB5_APP_ABI_V7 7u
#define TAB5_DIRECTORY_ENTRY_DIRECTORY 1u
struct tab5_app_keyboard_state {
 uint32_t size,modifiers,connected;
 /* USB HID keyboard usage bitmap: keys[usage/8] & (1u << (usage%8)). */
 uint8_t keys[32];
};
struct tab5_app_directory_entry {
 uint32_t size,flags,file_size;
 char name[256]; /* UTF-8 basename, including NUL; never truncated. */
};
struct tab5_app_api7 {
 struct tab5_app_api6 base;
 /* Set state.size first. Returns 1 on success, negative on error. Modifiers
  * use USB HID bits (left Ctrl/Shift/Alt/GUI, then right Ctrl/Shift/Alt/GUI),
  * not TAB5_MOD_*. All fields except size are zero while the canvas is hidden
  * or inactive; disconnected keyboards release their keys immediately. */
 int (*keyboard_state)(struct tab5_app_keyboard_state* state);
 /* Absolute canonical path. Scope is the selected argument's parent (or the
  * argument itself if a directory); without an argument it is /DOS. Returns
  * a positive handle sharing the eight open_read slots. Close with base.close.
  * No directory writes, process launches or raw disk writes are provided. */
 int (*directory_open)(const char* path);
 /* Set entry.size first. 1 entry, 0 end, negative error (including >1024
  * entries or an unrepresentable name/size). Directory file_size is zero. */
 int (*directory_next)(int handle,struct tab5_app_directory_entry* entry);
};
extern const struct tab5_app_api7* tab5_app_api_v7(void);
#ifdef __cplusplus
}
#endif

/* Optional bounded PSRAM stack, inspected before starting the application. */
#define TAB5_APP_STACK(bytes) __attribute__((section(".tab5.stack"), used, visibility("default"))) const uint32_t tab5_app_stack_bytes = (bytes)
