#pragma once
#include <pebble.h>

/* Global accent color — green normally, red/black during warn/blink.
 * All drawing modules read this instead of hardcoding GColorMalachite. */

void   theme_set_accent(GColor color);
GColor theme_accent_color(void);
