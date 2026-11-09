/* Feed one shell command at a time. A driver may spend seconds probing while
 * the BIOS type-ahead buffer holds only 15 keys; finishing pc_type is not a
 * signal that FreeCOM has consumed or executed that command. */
#pragma once
#include "keyboard.h"

static int dos_shell_prompt(void) {
    if (dos_keyboard_typing() || keyboard_pending() || (kbc_read(4) & 1) ||
        bda16(0x1a) != bda16(0x1c) || *bda(0x49) != 3) return 0;
    unsigned page = *bda(0x62), columns = bda16(0x4a);
    if (page >= 8 || columns != 80) return 0;
    unsigned cursor = bda16(0x50 + page * 2), row = cursor >> 8;
    if ((cursor & 255u) != 4 || row >= 25) return 0;
    unsigned offset = bda16(0x4e), cell = offset / 2u + row * columns;
    /* Four characters must fit inside the 256 KiB interleaved VGA planes. */
    if ((offset & 1u) || cell > 32764u) return 0;
    const uint8_t *line = vga_planes() + cell * 8u;
    if (line[0] < 'A' || line[0] > 'Z' || line[8] != ':' ||
        line[16] != '\\' || line[24] != '>') return 0;
    return line[0];
}

enum { DOS_STARTUP_IDLE, DOS_STARTUP_SOUND, DOS_STARTUP_MOUSE,
       DOS_STARTUP_LAYOUT, DOS_STARTUP_LAYOUT_WAIT, DOS_STARTUP_DRIVE,
       DOS_STARTUP_PROGRAM, DOS_STARTUP_DONE };
struct dos_startup { unsigned step; uint32_t since; };
static void dos_startup_begin(struct dos_startup *s) {
    s->step = DOS_STARTUP_SOUND; s->since = pc.frames;
}
/* 1 asks the frontend to switch layout; negative results stop automatic
 * input and leave the live DOS session available for manual commands. */
static int dos_startup_poll(struct dos_startup *s, int mouse_driver,
                            int french, int layout_pending, const char *program) {
    if (!s->step || s->step == DOS_STARTUP_DONE) return 0;
    if (pc.frames - s->since > 4200u) { s->step = DOS_STARTUP_DONE; return -2; }
    int drive = dos_shell_prompt();
    if (!drive) return 0;
    switch (s->step) {
    case DOS_STARTUP_SOUND: pc_type("set blaster=a220 i7 d1 t3\n"); break;
    case DOS_STARTUP_MOUSE: if (mouse_driver) pc_type("b:\\ctmouse /s14 /r11\n"); break;
    case DOS_STARTUP_LAYOUT:
        ++s->step; s->since = pc.frames;
        return french ? 1 : 0;
    case DOS_STARTUP_LAYOUT_WAIT:
        if (layout_pending) return 0;
        break;
    case DOS_STARTUP_DRIVE:
        if (!program[0]) { s->step = DOS_STARTUP_DONE; return 0; }
        pc_type("c:\n"); break;
    case DOS_STARTUP_PROGRAM: {
        if (drive != 'C') { s->step = DOS_STARTUP_DONE; return -1; }
        char command[16]; snprintf(command,sizeof(command),"%s\n",program);
        pc_type(command); break;
    }
    default: return 0;
    }
    ++s->step; s->since = pc.frames; return 0;
}
