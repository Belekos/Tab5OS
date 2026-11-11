#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define MACHINE_FB_WIDTH 1024
#define MACHINE_FB_HEIGHT 768
#define MACHINE_AUDIO_HZ 44100u
int machine_init(int mouse_driver);
void machine_step(void);
int machine_video(int capture);
int machine_stopped(void);
void machine_screen_size(int *width, int *height);
int machine_text_mode(void);
uint32_t machine_screen_signature(void);
void machine_render_line(int y, uint16_t *destination);
uint32_t machine_instructions(void);
void machine_key(uint16_t scan, int down);
void machine_mouse(int dx, int dy, uint8_t buttons);
void machine_type(const char *text);
int machine_typing(void);
int machine_screen_contains(const char *word);
int machine_shell_prompt(void);
uint32_t machine_keyboard_vector(void);
void machine_audio_render(int16_t *samples, unsigned frames);
void machine_cancel_input(void);
