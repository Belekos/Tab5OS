/* Execute the production Doom input adapter with host service/event stubs. */
#ifdef _MSC_VER
#define __attribute__(x)
#endif
#include <stdio.h>
#include <stdlib.h>
#include "../papp_i_system.c"

const app_services_t *_papp_svc;
volatile int papp_exit_requested;
uint32_t tab5_game_buttons;
gamestate_t gamestate;
boolean usergame, demoplayback;
int menuactive, gametic, realtic_clock_rate;
int snd_channels, snd_samplerate, snd_MusicVolume, snd_SfxVolume;
int key_right = KEYD_RIGHTARROW, key_left = KEYD_LEFTARROW;
int key_up = KEYD_UPARROW, key_down = KEYD_DOWNARROW;
int key_menu_right = KEYD_RIGHTARROW, key_menu_left = KEYD_LEFTARROW;
int key_menu_up = KEYD_UPARROW, key_menu_down = KEYD_DOWNARROW;
int key_fire = KEYD_RCTRL, key_speed = KEYD_RSHIFT, key_use = ' ';
int key_strafe = KEYD_RALT;
int key_enter = KEYD_ENTER, key_backspace = KEYD_BACKSPACE;
int key_escape = KEYD_ESCAPE, key_weapontoggle = '0', key_map = KEYD_TAB;
int key_strafeleft = ',', key_straferight = '.';

static uint32_t now_ms;
static event_t events[128];
static int event_count, saves, checks, mutate_menu;
#define CHECK(c) do { ++checks; if (!(c)) { \
    fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #c); exit(1); } } while (0)
#define BUTTON(n) (UINT32_C(1) << DOOM_BUTTON_##n)

void tab5_doom_audio(void) {}
void app_return_to_launcher(void) { abort(); }
const char *tab5_game_root(void) { return "/"; }
void G_SaveGame(int slot, char *description)
{
    CHECK(slot == 0 && !strcmp(description, "Tab5OS"));
    ++saves;
}
void D_PostEvent(event_t *event)
{
    CHECK(event_count < 128);
    events[event_count++] = *event;
    if (mutate_menu && event->type == ev_keydown && event->data1 == key_escape)
        menuactive = 1;
}
static int64_t get_time(void) { return (int64_t)now_ms * 1000; }
static void read_buttons(papp_gamepad_state_t *state) { memset(state, 0, sizeof(*state)); }
static void sleep_ms(int ms) { now_ms += (uint32_t)ms; }
static void reset(void)
{
    static app_services_t services;
    services.get_time_us = get_time;
    services.input_gamepad_read = read_buttons;
    services.delay_ms = sleep_ms;
    _papp_svc = &services;
    now_ms = 1234567890; /* Start long after the old ten-minute overflow. */
    I_Init();
    gametic = 3; gamestate = GS_LEVEL; usergame = 1; demoplayback = 0;
    menuactive = 0; tab5_game_buttons = 0; event_count = saves = mutate_menu = 0;
}
static void poll(uint32_t buttons) { tab5_game_buttons = buttons; I_StartTic(); }
static void event_is(int index, int key, int down)
{
    CHECK(index < event_count);
    CHECK(events[index].data1 == key);
    CHECK(events[index].type == (down ? ev_keydown : ev_keyup));
    CHECK(events[index].data2 == 0 && events[index].data3 == 0);
}

static void test_menus(void)
{
    reset(); gamestate = GS_DEMOSCREEN; usergame = 0; mutate_menu = 1;
    poll(BUTTON(A) | BUTTON(START));
    CHECK(event_count == 1); event_is(0, key_escape, 1);
    CHECK(menuactive == 1);
    poll(BUTTON(A)); CHECK(event_count == 1);
    poll(0); CHECK(event_count == 2); event_is(1, key_escape, 0);
    poll(BUTTON(START)); CHECK(event_count == 3); event_is(2, key_enter, 1);
    poll(BUTTON(START)); CHECK(event_count == 3);
    menuactive = 0; usergame = 1; gamestate = GS_LEVEL;
    poll(BUTTON(START)); CHECK(event_count == 3); /* No Use from a held confirmation. */
    poll(0); CHECK(event_count == 4); event_is(3, key_enter, 0);
    poll(BUTTON(START)); CHECK(event_count == 5); event_is(4, key_use, 1);

    reset(); usergame = 0; demoplayback = 1; /* Attract demo already playing. */
    poll(BUTTON(START)); CHECK(event_count == 1); event_is(0, key_escape, 1);
    poll(0); menuactive = 1;
    poll(BUTTON(A)); CHECK(event_count == 3); event_is(2, key_enter, 1);
    poll(0); poll(BUTTON(B)); event_is(4, key_backspace, 1);
    poll(0); poll(BUTTON(SELECT)); event_is(6, key_backspace, 1);

    reset(); menuactive = 1;
    int old_menu_up = key_menu_up;
    key_menu_up = 'i';
    poll(BUTTON(UP)); CHECK(event_count == 1); event_is(0, 'i', 1);
    key_menu_up = 'k';
    poll(0); event_is(1, 'i', 0); /* Release the original configured key. */
    poll(BUTTON(UP)); event_is(2, 'k', 1);
    key_menu_up = old_menu_up;
}

static void test_gameplay(void)
{
    reset(); gametic = 2;
    poll(BUTTON(UP)); CHECK(event_count == 0);
    gametic = 3; poll(BUTTON(UP)); CHECK(event_count == 1); event_is(0, key_up, 1);
    poll(0); event_is(1, key_up, 0);

    const unsigned buttons[] = {DOOM_BUTTON_RIGHT, DOOM_BUTTON_LEFT, DOOM_BUTTON_UP,
        DOOM_BUTTON_DOWN, DOOM_BUTTON_A, DOOM_BUTTON_B, DOOM_BUTTON_SELECT,
        DOOM_BUTTON_START, DOOM_BUTTON_MENU, DOOM_BUTTON_Y,
        DOOM_BUTTON_L, DOOM_BUTTON_R};
    const int keys[] = {key_right, key_left, key_up, key_down, key_fire, key_speed,
        key_weapontoggle, key_use, key_escape, key_map, key_strafeleft,
        key_straferight};
    for (unsigned i = 0; i < sizeof(buttons) / sizeof(buttons[0]); ++i) {
        reset(); poll(UINT32_C(1) << buttons[i]);
        int count = buttons[i] == DOOM_BUTTON_B ? 2 : 1;
        CHECK(event_count == count); event_is(0, keys[i], 1);
        if (count == 2) event_is(1, key_strafe, 1);
        poll(UINT32_C(1) << buttons[i]); CHECK(event_count == count);
        poll(0); CHECK(event_count == count * 2); event_is(count, keys[i], 0);
        if (count == 2) event_is(3, key_strafe, 0);
    }
    reset(); poll(BUTTON(B) | BUTTON(LEFT));
    CHECK(event_count == 3); event_is(0, key_left, 1); event_is(1, key_speed, 1);
    event_is(2, key_strafe, 1);

    reset(); poll(BUTTON(X) | BUTTON(SAVE));
    CHECK(saves == 1 && event_count == 0);
    poll(BUTTON(X)); CHECK(saves == 1 && event_count == 0);
    poll(0); poll(BUTTON(X)); CHECK(event_count == 0);
    reset(); poll(BUTTON(X)); /* Raw S can precede the UI's Ctrl+S request. */
    CHECK(event_count == 0);
    poll(BUTTON(X) | BUTTON(SAVE)); CHECK(saves == 1 && event_count == 0);
    reset(); menuactive = 1; poll(BUTTON(SAVE)); CHECK(saves == 0);
    reset(); demoplayback = 1; poll(BUTTON(SAVE)); CHECK(saves == 0);

    reset(); key_use = key_fire;
    poll(BUTTON(A) | BUTTON(START)); CHECK(event_count == 1); event_is(0, key_fire, 1);
    poll(BUTTON(START)); CHECK(event_count == 1);
    poll(0); CHECK(event_count == 2); event_is(1, key_fire, 0);
    key_use = ' ';

    reset(); gamestate = GS_INTERMISSION;
    poll(BUTTON(A) | BUTTON(START));
    CHECK(event_count == 2); event_is(0, key_fire, 1); event_is(1, key_use, 1);
    reset(); gamestate = GS_FINALE; usergame = 0;
    poll(BUTTON(A)); CHECK(event_count == 1); event_is(0, key_fire, 1);
}

static void test_clock(void)
{
    reset(); CHECK(I_GetTime() == 0 && I_GetTimeMS() == 0);
    const uint32_t elapsed[] = {1, 28, 29, 1000, 613566, 613567, 1227134,
        3600000, 86400000, 172800000};
    for (unsigned i = 0; i < sizeof(elapsed) / sizeof(elapsed[0]); ++i) {
        now_ms = UINT32_C(1234567890) + elapsed[i];
        CHECK(I_GetTime() == (int)((uint64_t)elapsed[i] * 35 / 1000));
        CHECK(I_GetTimeMS() == (int)elapsed[i]);
    }
    realtic_clock_rate = 200;
    CHECK(I_GetTime() == (int)((uint64_t)elapsed[9] * 70 / 1000));

    now_ms = UINT32_MAX - 10; I_Init();
    now_ms = 19; CHECK(I_GetTimeMS() == 30 && I_GetTime() == 1);
    now_ms = 1019; CHECK(I_GetTimeMS() == 1030 && I_GetTime() == 36);
}

int main(void)
{
    test_menus(); test_gameplay(); test_clock();
    printf("{\"passed\":true,\"assertions\":%d}\n", checks);
    return 0;
}
