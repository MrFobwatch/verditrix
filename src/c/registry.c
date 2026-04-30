#include "registry.h"

/* Forward declarations from each complication module */
extern Complication *comp_weather;
extern Complication *comp_date;
extern Complication *comp_battery;

static Complication **s_comps[] = {
    &comp_weather,
    &comp_date,
    &comp_battery,
};
#define N_COMPS (sizeof(s_comps) / sizeof(s_comps[0]))

static int s_current = 0;

void registry_init(void) {
    for (size_t i = 0; i < N_COMPS; i++) {
        complication_init(*s_comps[i]);
    }
}

void registry_deinit(void) {
    for (size_t i = 0; i < N_COMPS; i++) {
        complication_deinit(*s_comps[i]);
    }
}

void registry_next(void) {
    registry_set_current(s_current + 1);
}

void registry_tick(struct tm *t) {
    /* Tick all complications, not just active — keeps their cached state
     * fresh so no flicker when cycling to a stale one. */
    for (size_t i = 0; i < N_COMPS; i++) {
        complication_tick(*s_comps[i], t);
    }
}

void registry_draw(GContext *ctx, GRect bounds) {
    registry_draw_index(s_current, ctx, bounds);
}

void registry_draw_index(int idx, GContext *ctx, GRect bounds) {
    if (idx < 0 || idx >= (int)N_COMPS) idx = 0;
    complication_draw(*s_comps[idx], ctx, bounds);
}

void registry_set_current(int idx) {
    s_current = ((idx % (int)N_COMPS) + (int)N_COMPS) % (int)N_COMPS;
}

int registry_current_idx(void) {
    return s_current;
}

int registry_peek_next(void) {
    return (s_current + 1) % (int)N_COMPS;
}

const char* registry_current_name(void) {
    return (*s_comps[s_current])->name;
}
