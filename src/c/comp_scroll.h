#pragma once
#include <pebble.h>

/*
 * comp_scroll — animated complication scroll layer.
 *
 * Owns two slots: outgoing and incoming. When comp_scroll_trigger is called,
 * the outgoing complication slides left and the incoming slides in from the
 * right. The comp_mask layer above provides diagonal masking; this layer
 * draws freely across its full bounds.
 *
 * The done callback fires (with the new index) only when the animation
 * completes naturally. If a new trigger interrupts an in-progress animation,
 * the done callback for the interrupted animation is silently dropped.
 */

typedef void (*CompScrollDoneCb)(int new_idx);

void comp_scroll_init(Layer *parent, GRect diamond_bounds);
void comp_scroll_deinit(void);

/*
 * Start a left-slide from from_idx to to_idx.
 * Safe to call during a transition — the current animation is cancelled and
 * a fresh one starts from from_idx.
 */
void comp_scroll_trigger(int from_idx, int to_idx, CompScrollDoneCb cb);

void comp_scroll_mark_dirty(void);
