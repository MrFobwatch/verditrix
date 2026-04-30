#pragma once
#include <pebble.h>

/*
 * config.h — single source of truth for visual identity and timing.
 * Colors and animation timings live here so re-theming touches one file.
 */

/* === Colors === */
#define OMNI_BG          GColorLightGray      /* watch background */
#define OMNI_TIME_TEXT   GColorBlack          /* time numerals on the X-halves */
#define OMNI_COMP_TEXT   GColorWhite          /* complication text inside diamond */

/* === Animation timings === */
#define DOOR_ANIM_MS         400              /* open/close slide duration */
#define HOLD_DURATION_MS     3000             /* how long complication is visible */
#define BLINK_INTERVAL_MS    50               /* ms per blink/warn frame */
#define NUM_BLINK_FRAMES     4                /* frames for BLINK_OUT / BLINK_IN */
#define NUM_WARN_FRAMES      40               /* frames for WARN (40 * 50ms = 2s) */

/* === X-half door geometry (as % of diamond half-width/height) === */
#define X_WING_PCT        95                  /* wing corner X: 95% of half-width */
#define X_WAIST_PCT       12                  /* waist X at rest: tight X pinch */

/* === Frame / bezel geometry === */
#define FRAME_INSET          4               /* screen edge → outer rect */
#define DIAMOND_INSET        0               /* screen edge → diamond bounding rect */
#define DIAMOND_INSET_H      0               /* screen edge → diamond bounding rect (vertical) */
#define NODE_RADIUS          5               /* bezel node dot radius */
