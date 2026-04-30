#pragma once
#include "complication.h"

/*
 * registry.h — owns the array of available complications.
 *
 * Adding a new complication is two steps:
 *   1. Create complications/comp_yourname.c with `extern Complication *comp_yourname;`
 *      defined as a pointer to your concrete instance's super field.
 *   2. Add `extern Complication *comp_yourname;` and reference it in
 *      the s_comps[] array in registry.c.
 *
 * That's the entire extensibility surface.
 */

void registry_init(void);
void registry_deinit(void);

/* Cycle to next complication. Caller is responsible for triggering redraw. */
void registry_next(void);

/* Forward tick events to active complication. */
void registry_tick(struct tm *t);

/* Render active complication into the given bounds. */
void registry_draw(GContext *ctx, GRect bounds);

/* For diagnostics */
const char* registry_current_name(void);
