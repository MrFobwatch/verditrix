#pragma once
#include "complication.h"

void registry_init(void);
void registry_deinit(void);

/* Cycle to next complication. Caller is responsible for triggering redraw. */
void registry_next(void);

/* Forward tick events to all complications. */
void registry_tick(struct tm *t);

/* Render active complication into the given bounds. */
void registry_draw(GContext *ctx, GRect bounds);

/* Render a specific complication by index. idx is clamped to [0, N-1]. */
void registry_draw_index(int idx, GContext *ctx, GRect bounds);

/* Set the active complication index directly (called by scroll done cb). */
void registry_set_current(int idx);

/* Return the current active index. */
int registry_current_idx(void);

/* Return what the next index would be after cycling (does not change state). */
int registry_peek_next(void);

/* For diagnostics */
const char *registry_current_name(void);
