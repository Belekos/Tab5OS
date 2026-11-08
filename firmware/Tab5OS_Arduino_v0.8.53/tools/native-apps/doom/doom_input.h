#ifndef TAB5_DOOM_INPUT_H
#define TAB5_DOOM_INPUT_H

#include <stdint.h>

/* Positions in the Tab5OS buttons() mask, including one-shot OS requests. */
enum {
    DOOM_BUTTON_RIGHT, DOOM_BUTTON_LEFT, DOOM_BUTTON_UP, DOOM_BUTTON_DOWN,
    DOOM_BUTTON_A, DOOM_BUTTON_B, DOOM_BUTTON_SELECT, DOOM_BUTTON_START,
    DOOM_BUTTON_EXIT, DOOM_BUTTON_SAVE, DOOM_BUTTON_MENU,
    DOOM_BUTTON_X, DOOM_BUTTON_Y, DOOM_BUTTON_L, DOOM_BUTTON_R,
    DOOM_BUTTON_B_STRAFE, /* Internal companion to B; not an ABI button. */
    DOOM_BUTTON_COUNT
};
enum { DOOM_INPUT_PLAY, DOOM_INPUT_MENU, DOOM_INPUT_DEMO };

typedef struct {
    /* A held button owns the key selected at press time, even if a menu closes. */
    int held[DOOM_BUTTON_COUNT]; /* key code + 1; zero means no key */
    uint32_t previous;
} doom_input_state_t;

typedef void (*doom_input_event_fn)(int key, int down, void *context);

static int doom_input_contains(const int *held, int key)
{
    for (int i = 0; i < DOOM_BUTTON_COUNT; ++i)
        if (held[i] == key) return 1;
    return 0;
}

static void doom_input_update(doom_input_state_t *state, uint32_t buttons,
                              int mode, const int bindings[DOOM_BUTTON_COUNT][3],
                              doom_input_event_fn post, void *context)
{
    int previous[DOOM_BUTTON_COUNT];
    for (int i = 0; i < DOOM_BUTTON_COUNT; ++i) {
        uint32_t bit = UINT32_C(1) << i;
        previous[i] = state->held[i];
        if (!(buttons & bit)) state->held[i] = 0;
        else if (!(state->previous & bit)) state->held[i] = bindings[i][mode] + 1;
    }
    state->previous = buttons;

    /* A/START can own the same menu key. Release it only after both let go. */
    for (int i = 0; i < DOOM_BUTTON_COUNT; ++i) {
        int key = previous[i];
        if (key && !doom_input_contains(state->held, key)) {
            post(key - 1, 0, context);
            for (int j = i + 1; j < DOOM_BUTTON_COUNT; ++j)
                if (previous[j] == key) previous[j] = 0;
        }
    }
    for (int i = 0; i < DOOM_BUTTON_COUNT; ++i) {
        int key = state->held[i];
        if (key && !doom_input_contains(previous, key)) {
            int seen = 0;
            for (int j = 0; j < i; ++j) seen |= state->held[j] == key;
            if (!seen) post(key - 1, 1, context);
        }
    }
}

typedef struct {
    uint64_t elapsed_ms;
    uint32_t previous_ms;
    int initialized;
} doom_clock_t;

static uint64_t doom_clock_update(doom_clock_t *clock, uint32_t now_ms)
{
    if (clock->initialized)
        clock->elapsed_ms += (uint32_t)(now_ms - clock->previous_ms);
    clock->previous_ms = now_ms;
    clock->initialized = 1;
    return clock->elapsed_ms;
}

static int doom_clock_tics(uint64_t elapsed_ms, int ticrate, int clock_rate)
{
    return (int)(elapsed_ms * (uint64_t)ticrate * (uint64_t)clock_rate / 100000);
}

#endif
