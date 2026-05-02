#pragma once
#include <pebble.h>

/*
 * config.h — single source of truth for visual identity and timing.
 * Colors and animation timings live here so re-theming touches one file.
 */

/* === Colors === */
#define OMNI_BG          GColorLightGray      /* watch background */
#define OMNI_TIME_TEXT   GColorBlack          /* time numerals on the X-halves */
#define OMNI_COMP_TEXT   GColorBlack          /* complication text inside diamond */

/* === Animation timings === */
#define DOOR_ANIM_MS         400              /* door open slide duration */
#define DOOR_CLOSE_MS        700              /* door close slide duration */
#define HOLD_DURATION_MS     5000             /* how long complication is visible on first open */
#define TAP_HOLD_DURATION_MS 4000             /* hold duration reset after each tap-to-scroll */
#define BLINK_INTERVAL_MS    120              /* ms per blink frame (BLINK_OUT) */
#define NUM_BLINK_FRAMES     3                /* frames for BLINK_OUT */
#define NUM_WARN_FRAMES      20               /* frames for WARN */
#define WARN_STEP_MS         15               /* added per-frame to slow warn blink over time */
#define MASK_OVERSHOOT       0                /* px mask extends past diamond edge to cover stroke */
#define SCROLL_ANIM_MS       350             /* complication slide duration */
#define ROT_FRAME_MS         35              /* ms per frame for node rotation */
#define ROT_CW_FRAMES        10              /* frames per CW notch (matches SCROLL_ANIM_MS) */

/* === X-half door geometry (as % of diamond half-width/height) === */
#define X_WING_PCT        95                  /* wing corner X: 95% of half-width */
#define X_WAIST_PCT       12                  /* waist X at rest: tight X pinch */

/* === Frame / bezel geometry === */
#define FRAME_INSET          2               /* screen edge → outer rect (emery) */
#define FRAME_INSET_ROUND    1               /* screen edge → circle border (gabbro) */
#define DIAMOND_INSET        0               /* screen edge → diamond bounding rect */
#define DIAMOND_INSET_H      0               /* screen edge → diamond bounding rect (vertical) */
#define NODE_RADIUS          5               /* bezel node dot radius */
