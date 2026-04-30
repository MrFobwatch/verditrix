#include "theme.h"
#include "config.h"

static GColor s_accent = { .argb = GColorMalachiteARGB8 };

void theme_set_accent(GColor color) { s_accent = color; }
GColor theme_accent_color(void)     { return s_accent; }
