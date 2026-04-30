#include "complication.h"
#include "config.h"

/*
 * comp_date.c — example concrete complication.
 *
 * To add another complication:
 *   1. Copy this file to comp_yourname.c
 *   2. Rename the struct, vtable, instance, and exported pointer
 *   3. Implement init/deinit/tick/draw
 *   4. Register it in registry.c
 */

typedef struct {
    Complication super;
    char day_buf[8];     /* "MON", "TUE", ... */
    char date_buf[16];   /* "APR 25" */
} DateComplication;

static void d_init(Complication *base) {
    DateComplication *self = (DateComplication *)base;
    self->day_buf[0]  = '\0';
    self->date_buf[0] = '\0';
}

static void d_deinit(Complication *base) {
    (void)base;  /* nothing to free */
}

static void d_tick(Complication *base, struct tm *t) {
    DateComplication *self = (DateComplication *)base;
    if (!t) return;
    strftime(self->day_buf,  sizeof(self->day_buf),  "%a",     t);
    strftime(self->date_buf, sizeof(self->date_buf), "%b %d",  t);
    /* Uppercase for that HUD-readout feel */
    for (char *p = self->day_buf;  *p; p++) if (*p >= 'a' && *p <= 'z') *p -= 32;
    for (char *p = self->date_buf; *p; p++) if (*p >= 'a' && *p <= 'z') *p -= 32;
}

static void d_draw(Complication *base, GContext *ctx, GRect bounds) {
    DateComplication *self = (DateComplication *)base;

    /*
     * Diamond reveal area is rhombus-shaped, but a complication's safe
     * rendering zone is the inscribed rectangle — roughly 60% of the
     * bounding box centered on it. Anything drawn outside this zone
     * risks being clipped by the door warp at intermediate progress.
     */
    int cx = bounds.origin.x + bounds.size.w / 2;
    int cy = bounds.origin.y + bounds.size.h / 2;

    GFont big   = fonts_get_system_font(FONT_KEY_LECO_28_LIGHT_NUMBERS);
    GFont small = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);

    graphics_context_set_text_color(ctx, OMNI_COMP_TEXT);

    GRect day_rect  = GRect(cx - 60, cy - 40, 120, 32);
    GRect date_rect = GRect(cx - 70, cy - 5,  140, 36);

    graphics_draw_text(ctx, self->day_buf, small, day_rect,
                       GTextOverflowModeFill, GTextAlignmentCenter, NULL);
    graphics_draw_text(ctx, self->date_buf, big, date_rect,
                       GTextOverflowModeFill, GTextAlignmentCenter, NULL);
}

static const ComplicationVTable s_vtable = {
    .init   = d_init,
    .deinit = d_deinit,
    .tick   = d_tick,
    .draw   = d_draw,
};

static DateComplication s_instance = {
    .super = { .vtable = &s_vtable, .name = "Date" }
};

Complication *comp_date = &s_instance.super;
