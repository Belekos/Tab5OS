/*
 * Doom I_System + I_StartTic shim for PSRAM App.
 *
 * System timing, input polling, and exit handling routed through
 * the app_services_t service table.
 */
#include "psram_app.h"
#include <string.h>
#include <stdint.h>
#include "doom_input.h"

/* prboom engine headers */
#include <doomdef.h>
#include <doomstat.h>
#include <d_event.h>
#include <d_main.h>
#include <g_game.h>
#include <i_main.h>
#include <i_system.h>
#include <r_fps.h>
#include <s_sound.h>

extern const app_services_t *_papp_svc;
extern volatile int papp_exit_requested;
extern void app_return_to_launcher(void);

/* ── Input keymap ────────────────────────────────────────────────────── */

/* Map the Tab5OS button snapshot to Doom's configurable key globals. */
static doom_input_state_t input_state;
static doom_clock_t input_clock;

static void post_key(int key, int down, void *context)
{
    event_t event = {0};
    (void)context;
    event.type = down ? ev_keydown : ev_keyup;
    event.data1 = key;
    D_PostEvent(&event);
}

/* ── I_StartTic — Input polling ──────────────────────────────────────── */

void I_StartTic(void)
{
    extern void tab5_doom_audio(void);
    tab5_doom_audio();
    papp_gamepad_state_t state;

    _papp_svc->input_gamepad_read(&state);
    extern uint32_t tab5_game_buttons;

    /* Check for exit request (watchdog already handles long press) */
    if (papp_exit_requested) {
        app_return_to_launcher();
    }

    /* D_PostEvent drops startup events; do not consume a held button yet. */
    if (gametic < 3) return;

    /* Snapshot context before posting: responders can open/close a menu. */
    int mode = menuactive ? DOOM_INPUT_MENU :
        (demoplayback || gamestate == GS_DEMOSCREEN)
            ? DOOM_INPUT_DEMO : DOOM_INPUT_PLAY;
    const int bindings[DOOM_BUTTON_COUNT][3] = {
        {key_right,        key_menu_right, key_menu_right},
        {key_left,         key_menu_left,  key_menu_left},
        {key_up,           key_menu_up,    key_menu_up},
        {key_down,         key_menu_down,  key_menu_down},
        {key_fire,         key_enter,      key_escape}, /* A */
        {key_speed,        key_backspace,  key_escape}, /* B */
        {key_weapontoggle, key_backspace,  key_escape}, /* SELECT */
        {key_use,          key_enter,      key_escape}, /* START */
        {-1, -1, -1}, /* Exit is handled by the app service. */
        {-1, -1, -1}, /* Save is an OS request, not a held key. */
        {key_escape,       key_escape,     key_escape}, /* MENU */
        {-1, -1, -1}, /* X/S is also asserted before the OS delivers Ctrl+S. */
        {key_map,          -1,             -1},         /* Y */
        {key_strafeleft,   -1,             -1},         /* L */
        {key_straferight,  -1,             -1},         /* R */
        {key_strafe,       -1,             -1},         /* B's original strafe modifier */
    };
    uint32_t buttons = tab5_game_buttons;
    if (buttons & (UINT32_C(1) << DOOM_BUTTON_B))
        buttons |= UINT32_C(1) << DOOM_BUTTON_B_STRAFE;
    if (buttons & (UINT32_C(1) << DOOM_BUTTON_SAVE)) {
        if (mode == DOOM_INPUT_PLAY && gamestate == GS_LEVEL)
            G_SaveGame(0, "Tab5OS");
    }
    doom_input_update(&input_state, buttons, mode, bindings, post_key, NULL);
}

/* ── Timing ──────────────────────────────────────────────────────────── */

int I_GetTimeMS(void)
{
    return (int)doom_clock_update(&input_clock,
        (uint32_t)(_papp_svc->get_time_us() / 1000));
}

int I_GetTime(void)
{
    uint64_t elapsed = doom_clock_update(&input_clock,
        (uint32_t)(_papp_svc->get_time_us() / 1000));
    return doom_clock_tics(elapsed, TICRATE, realtic_clock_rate);
}

void I_uSleep(unsigned long usecs)
{
    /* Convert to ms for the service call, minimum 1ms */
    int ms = (int)(usecs / 1000);
    if (ms < 1) ms = 1;
    _papp_svc->delay_ms(ms);
}

/* ── System Init / Exit ──────────────────────────────────────────────── */

void I_Init(void)
{
    extern int snd_channels;
    extern int snd_samplerate;

    memset(&input_state, 0, sizeof(input_state));
    memset(&input_clock, 0, sizeof(input_clock));
    (void)I_GetTimeMS();

    snd_channels = 8;
    snd_samplerate = 22050;
    snd_MusicVolume = 15;
    snd_SfxVolume = 15;
    realtic_clock_rate = 100;  /* normal speed */
}

void I_SafeExit(int rc)
{
    app_return_to_launcher();
    /* longjmp in app_return_to_launcher means we never reach here */
    for (;;) {}
}

const char *I_DoomExeDir(void)
{
    extern const char* tab5_game_root(void);
    return tab5_game_root();
}

const char *I_SigString(char *buf, size_t sz, int signum)
{
    if (buf && sz > 0) buf[0] = '\0';
    return buf;
}
