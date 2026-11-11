/* All Tiny386 state stays on the native application's worker. The frontend
 * never writes guest memory to inject input or to bypass DOS services. */
#include "machine.h"
#include "core/pc.h"
#include "../common/runtime.h"
#include <string.h>
#include <stdlib.h>

extern int dos_french_keyboard;
extern uint16_t dos_ascii_key(unsigned char ch);
extern int dos_key_held(uint16_t scan);
static PC *machine;
static uint16_t *framebuffer;
static int view_x, view_y, view_w = 720, view_h = 400, text_mode = 1;
static uint32_t screen_serial, typed_at;
static char typed[256];
static unsigned typed_pos, typed_len;

static void redrawn(void *opaque, int x, int y, int w, int h)
{
    (void)opaque; (void)x; (void)y; (void)w; (void)h;
    ++screen_serial;
}

int machine_init(int mouse_driver)
{
    framebuffer = calloc(MACHINE_FB_WIDTH * MACHINE_FB_HEIGHT, sizeof(*framebuffer));
    if (!framebuffer) return -1;
    PCConfig conf = {0};
    conf.mem_size = 8 * 1024 * 1024;
    conf.vga_mem_size = 1024 * 1024;
    conf.width = MACHINE_FB_WIDTH; conf.height = MACHINE_FB_HEIGHT;
    conf.cpu_gen = 6; conf.fpu = 1; conf.fill_cmos = 1;
    conf.bios = "/System/DOS386/bios.bin";
    conf.vga_bios = "/System/DOS386/vgabios.bin";
    conf.fdd[0] = "/__dos386/fd0";
    conf.fdd[1] = mouse_driver ? "/__dos386/fd1" : NULL;
    conf.disks[0] = "/__dos386/hd0";
    machine = pc_new(redrawn, NULL, (uint8_t *)framebuffer, &conf);
    if (!machine) return -1;
    load_bios_and_reset(machine);
    typed_pos = typed_len = 0; typed_at = tab5_api->base.millis();
    return 0;
}

static uint16_t ascii_key(unsigned char ch)
{
    if (dos_french_keyboard) return dos_ascii_key(ch);
    static const char normal[] = "\x1b" "1234567890-=\b\tqwertyuiop[]\n\0asdfghjkl;'`\0\\zxcvbnm,./";
    static const char shifted[] = "\x1b" "!@#$%^&*()_+\b\tQWERTYUIOP{}\n\0ASDFGHJKL:\"~\0|ZXCVBNM<>?";
    if (ch == ' ') return 0x39;
    if (ch == '\r') ch = '\n';
    for (unsigned i = 0; i < sizeof(normal) - 1; ++i) {
        if (normal[i] && (unsigned char)normal[i] == ch) return (uint16_t)(i + 1);
        if (shifted[i] && (unsigned char)shifted[i] == ch) return (uint16_t)(0x100u | (i + 1));
    }
    return 0;
}

void machine_key(uint16_t scan, int down)
{
    if (machine) ps2_put_keycode(machine->kbd, down, scan);
}
void machine_mouse(int dx, int dy, uint8_t buttons)
{
    if (machine) ps2_mouse_event(machine->mouse, dx, dy, 0, buttons & 3u);
}
void machine_type(const char *text)
{
    if (typed_len) return;
    size_t n = strlen(text);
    if (n >= sizeof(typed)) n = sizeof(typed) - 1;
    memcpy(typed, text, n); typed[n] = 0;
    typed_pos = 0; typed_len = (unsigned)n;
}
int machine_typing(void)
{
    return typed_len || (machine && ps2_keyboard_pending(machine->kbd));
}
void machine_cancel_input(void)
{
    typed_len = typed_pos = 0;
    machine_mouse(0, 0, 0);
}
static void type_step(void)
{
    uint32_t now = tab5_api->base.millis();
    if (!typed_len || now - typed_at < 20u || ps2_keyboard_pending(machine->kbd) ||
        (kbd_read_status(machine->i8042, 0x64) & 1u)) return;
    /* One character at a time avoids overrunning the guest's BIOS buffer. */
    const uint8_t *ram = (const uint8_t *)machine->phys_mem;
    unsigned head = ram[0x41a] | (unsigned)ram[0x41b] << 8;
    unsigned tail = ram[0x41c] | (unsigned)ram[0x41d] << 8;
    unsigned next = tail + 2; if (next >= 0x3e) next = 0x1e;
    if (next == head) return;
    uint16_t key = ascii_key((unsigned char)typed[typed_pos]);
    int shift = (key & 0x100u) && !dos_key_held(42) && !dos_key_held(54);
    int altgr = (key & 0x200u) && !dos_key_held(0xe038);
    if (shift) machine_key(42, 1);
    if (altgr) machine_key(0xe038, 1);
    if (key & 255u) { machine_key(key & 255u, 1); machine_key(key & 255u, 0); }
    if (shift) machine_key(42, 0);
    if (altgr) machine_key(0xe038, 0);
    if (++typed_pos >= typed_len) typed_len = typed_pos = 0;
    typed_at = now;
}
void machine_step(void)
{
    if (!machine) return;
    type_step(); pc_step(machine);
}
int machine_video(int capture)
{
    if (!machine) return 0;
    int changed = pc_vga_step_capture(machine, capture);
    if (changed) vga_view(machine->vga, &view_x, &view_y, &view_w, &view_h, &text_mode);
    return changed;
}
/* The shutdown port matches eight bytes; a partial prefix is not shutdown. */
int machine_stopped(void) { return machine && machine->shutdown_state == 8; }
void machine_screen_size(int *width, int *height) { *width = view_w; *height = view_h; }
int machine_text_mode(void) { return text_mode; }
uint32_t machine_screen_signature(void) { return screen_serial; }
void machine_render_line(int y, uint16_t *destination)
{
    if (view_w <= 0 || view_w > MACHINE_FB_WIDTH) return;
    if (y < 0 || y >= view_h || view_x < 0 || view_y < 0 ||
        view_x > MACHINE_FB_WIDTH - view_w || y >= MACHINE_FB_HEIGHT ||
        view_y >= MACHINE_FB_HEIGHT - y) {
        memset(destination, 0, (size_t)view_w * 2); return;
    }
    memcpy(destination, framebuffer + (view_y + y) * MACHINE_FB_WIDTH + view_x,
           (size_t)view_w * sizeof(*destination));
}
uint32_t machine_instructions(void)
{
    return machine ? (uint32_t)cpui386_get_cycle(machine->cpu) : 0;
}
static unsigned bda16(unsigned off)
{
    const uint8_t *ram = (const uint8_t *)machine->phys_mem;
    return ram[0x400 + off] | (unsigned)ram[0x401 + off] << 8;
}
int machine_screen_contains(const char *word)
{
    if (!machine || ((uint8_t *)machine->phys_mem)[0x449] != 3) return 0;
    const uint8_t *v = (const uint8_t *)machine->vga_mem;
    size_t len = strlen(word);
    if (len > 2000) return 0;
    for (size_t i = 0; i + len <= 2000; ++i) {
        size_t j = 0;
        while (j < len && v[(i + j) * 4] == (unsigned char)word[j]) ++j;
        if (j == len) return 1;
    }
    return 0;
}
int machine_shell_prompt(void)
{
    if (!machine || machine_typing() || (kbd_read_status(machine->i8042, 0x64) & 1u) ||
        bda16(0x1a) != bda16(0x1c)) return 0;
    const uint8_t *ram = (const uint8_t *)machine->phys_mem;
    if (ram[0x449] != 3 || ram[0x462] >= 8 || bda16(0x4a) != 80) return 0;
    unsigned cursor = bda16(0x50 + ram[0x462] * 2), row = cursor >> 8;
    if ((cursor & 255u) != 4 || row >= 25) return 0;
    unsigned offset = bda16(0x4e), cell = offset / 2u + row * 80u;
    if ((offset & 1u) || cell > 32764u) return 0;
    const uint8_t *line = (const uint8_t *)machine->vga_mem + cell * 4;
    return line[0] >= 'A' && line[0] <= 'Z' && line[4] == ':' && line[8] == '\\' &&
           line[12] == '>' ? line[0] : 0;
}
uint32_t machine_keyboard_vector(void)
{
    static const uint8_t vectors[] = {9, 0x15, 0x16, 0x2f};
    const uint8_t *ram = (const uint8_t *)machine->phys_mem;
    uint32_t h = 2166136261u;
    for (unsigned i = 0; i < sizeof(vectors); ++i)
        for (unsigned j = 0; j < 4; ++j) h = (h ^ ram[vectors[i] * 4 + j]) * 16777619u;
    return h;
}
void machine_audio_render(int16_t *samples, unsigned frames)
{
    if (!machine || frames > 512u) { if (frames <= 512u) memset(samples, 0, frames * 4u); return; }
    memset(samples, 0, frames * 4u);
    mixer_callback(machine, (uint8_t *)samples, (int)(frames * 4u));
}
