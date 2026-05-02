#pragma once
#include <pebble.h>

/*
 * frame.h — chrome decoration: outer rect, L-brackets, 8 rotating nodes.
 * Reads theme_accent_color() so it flashes in sync during WARN/BLINK.
 *
 * Rotation model:
 *   frame_rotate_cw()  — one clockwise notch per complication scroll
 *   frame_rotate_ccw() — unwind all accumulated CW rotation (spring release)
 */

void frame_init(Layer *parent);
void frame_deinit(void);

void frame_rotate_cw(void);
void frame_rotate_ccw(void);

void frame_mark_dirty(void);
