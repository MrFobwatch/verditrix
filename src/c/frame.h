#pragma once
#include <pebble.h>

/*
 * frame.h — chrome decoration: outer rect, L-brackets, 8 rotating nodes.
 * Reads theme_accent_color() so it flashes in sync during WARN/BLINK.
 */

void frame_init(Layer *parent);
void frame_deinit(void);

/* Trigger one quarter-turn node rotation (call on idle entry). */
void frame_start_rotation(void);

/* Force a redraw — call after theme accent changes. */
void frame_mark_dirty(void);
