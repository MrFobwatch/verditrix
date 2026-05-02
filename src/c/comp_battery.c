#include "complication.h"
#include "config.h"

/*
 * comp_battery.c — second example. Reads from battery_state_service.
 */

typedef struct {
    Complication super;
    int  percent;
    bool charging;
    char buf[8];   /* "100%" */
} BatteryComplication;

static void b_refresh(BatteryComplication *self) {
    BatteryChargeState s = battery_state_service_peek();
    self->percent  = s.charge_percent;
    self->charging = s.is_charging;
    snprintf(self->buf, sizeof(self->buf), "%d %%", self->percent);
}

static void b_init(Complication *base) {
    BatteryComplication *self = (BatteryComplication *)base;
    b_refresh(self);
}

static void b_deinit(Complication *base) { (void)base; }

static void b_tick(Complication *base, struct tm *t) {
    (void)t;
    b_refresh((BatteryComplication *)base);
}

static void b_draw(Complication *base, GContext *ctx, GRect bounds) {
    BatteryComplication *self = (BatteryComplication *)base;
    b_refresh(self);

    int cx = bounds.origin.x + bounds.size.w / 2;
    int cy = bounds.origin.y + bounds.size.h / 2;
    int u  = (bounds.size.w < bounds.size.h ? bounds.size.w : bounds.size.h) / 10;

    GColor bar_color;
    if      (self->percent > 50) bar_color = GColorMalachite;
    else if (self->percent > 20) bar_color = GColorYellow;
    else                          bar_color = GColorRed;

    GFont font = fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD);
    graphics_context_set_text_color(ctx, OMNI_COMP_TEXT);
    GRect text_rect = GRect(cx - u * 7 / 2, cy - u * 2, u * 7, u * 3);
    graphics_draw_text(ctx, self->buf, font, text_rect,
                       GTextOverflowModeFill, GTextAlignmentCenter, NULL);

    int bar_w = u * 6;
    int bar_h = u / 2 > 4 ? u / 2 : 4;
    int bar_x = cx - bar_w / 2;
    int bar_y = cy + u;

    graphics_context_set_stroke_color(ctx, OMNI_COMP_TEXT);
    graphics_context_set_stroke_width(ctx, 1);
    graphics_draw_rect(ctx, GRect(bar_x, bar_y, bar_w, bar_h));

    int fill_w = (self->percent * (bar_w - 2)) / 100;
    if (fill_w > 0) {
        graphics_context_set_fill_color(ctx, bar_color);
        graphics_fill_rect(ctx, GRect(bar_x + 1, bar_y + 1, fill_w, bar_h - 2),
                           0, GCornerNone);
    }
}

static const ComplicationVTable s_vtable = {
    .init   = b_init,
    .deinit = b_deinit,
    .tick   = b_tick,
    .draw   = b_draw,
};

static BatteryComplication s_instance = {
    .super = { .vtable = &s_vtable, .name = "Battery" }
};

Complication *comp_battery = &s_instance.super;
