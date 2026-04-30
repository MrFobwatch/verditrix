#include "complication.h"
#include "config.h"

typedef struct {
    Complication super;
    int  temp;
    char conditions[32];
} WeatherComplication;

static void w_init(Complication *base) {
    WeatherComplication *self = (WeatherComplication *)base;
    self->temp = 0;
    self->conditions[0] = '\0';
}

static void w_deinit(Complication *base) { (void)base; }

static void w_tick(Complication *base, struct tm *t) { (void)base; (void)t; }

static void w_draw(Complication *base, GContext *ctx, GRect bounds) {
    WeatherComplication *self = (WeatherComplication *)base;

    int cx = bounds.origin.x + bounds.size.w / 2;
    int cy = bounds.origin.y + bounds.size.h / 2;

    char temp_str[8];
    snprintf(temp_str, sizeof(temp_str), "%d\xc2\xb0", self->temp);

    GFont temp_font = fonts_get_system_font(FONT_KEY_LECO_28_LIGHT_NUMBERS);
    GFont cond_font = fonts_get_system_font(FONT_KEY_ROBOTO_CONDENSED_21);

    graphics_context_set_text_color(ctx, OMNI_COMP_TEXT);

    GRect temp_rect = GRect(cx - 40, cy - 30, 80, 36);
    GRect cond_rect = GRect(cx - 44, cy + 10, 88, 28);

    graphics_draw_text(ctx, temp_str, temp_font, temp_rect,
                       GTextOverflowModeFill, GTextAlignmentCenter, NULL);
    if (self->conditions[0]) {
        graphics_draw_text(ctx, self->conditions, cond_font, cond_rect,
                           GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    }
}

static const ComplicationVTable s_vtable = {
    .init   = w_init,
    .deinit = w_deinit,
    .tick   = w_tick,
    .draw   = w_draw,
};

static WeatherComplication s_instance = {
    .super = { .vtable = &s_vtable, .name = "Weather" }
};

Complication *comp_weather = &s_instance.super;

void comp_weather_set(int temp, const char *cond) {
    if (temp != INT32_MIN) s_instance.temp = temp;
    if (cond) snprintf(s_instance.conditions, sizeof(s_instance.conditions), "%s", cond);
}
