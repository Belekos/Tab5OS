/* Wait for a consumed command and a real FreeCOM prompt before sending the
 * next one; finishing the host type queue does not prove DOS is ready. */
#pragma once
enum { DOS_STARTUP_IDLE, DOS_STARTUP_SOUND, DOS_STARTUP_MOUSE,
       DOS_STARTUP_LAYOUT, DOS_STARTUP_LAYOUT_WAIT, DOS_STARTUP_DRIVE,
       DOS_STARTUP_PROGRAM, DOS_STARTUP_DONE };
struct dos_startup { unsigned step; uint32_t since; };
static void dos_startup_begin(struct dos_startup *s) {
    s->step = DOS_STARTUP_SOUND; s->since = base->millis();
}
static int dos_startup_poll(struct dos_startup *s, int mouse_driver,
                            int french, int layout_pending, const char *program) {
    if (!s->step || s->step == DOS_STARTUP_DONE) return 0;
    if (base->millis() - s->since > 120000u) { s->step = DOS_STARTUP_DONE; return -2; }
    int drive = machine_shell_prompt();
    if (!drive) return 0;
    switch (s->step) {
    case DOS_STARTUP_SOUND: machine_type("set blaster=a220 i5 d1 h5 t6\n"); break;
    case DOS_STARTUP_MOUSE: if (mouse_driver) machine_type("b:\\ctmouse /r11\n"); break;
    case DOS_STARTUP_LAYOUT:
        ++s->step; s->since = base->millis();
        return french ? 1 : 0;
    case DOS_STARTUP_LAYOUT_WAIT:
        if (layout_pending) return 0;
        break;
    case DOS_STARTUP_DRIVE:
        if (!program[0]) { s->step = DOS_STARTUP_DONE; return 0; }
        machine_type("c:\n"); break;
    case DOS_STARTUP_PROGRAM: {
        if (drive != 'C') { s->step = DOS_STARTUP_DONE; return -1; }
        char command[16]; snprintf(command, sizeof(command), "%s\n", program);
        machine_type(command); break;
    }
    default: return 0;
    }
    ++s->step; s->since = base->millis(); return 0;
}
