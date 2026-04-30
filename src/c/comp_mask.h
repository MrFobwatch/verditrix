#pragma once
#include <pebble.h>

/*
 * comp_mask — static inverse mask layer.
 *
 * Draws four filled triangles in OMNI_BG over the non-diamond corner areas,
 * so complication content drawn outside the diamond rhombus is hidden
 * regardless of what the complication renders.
 *
 * Layer is added as a child of parent and sits above the comp_scroll layer
 * but below the door layer. It never needs to be invalidated after init.
 */

void comp_mask_init(Layer *parent, GRect diamond_bounds);
void comp_mask_deinit(void);
