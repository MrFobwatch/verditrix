#include "frame.h"
#include "config.h"
#include "theme.h"

static Layer    *s_layer      = NULL;
static AppTimer *s_rot_timer  = NULL;
static int       s_rot_frame  = 0;
static int       s_node_offset = 0;

/* Returns a point on the perimeter of the rectangle (w x h) inset by `inset`,
 * at arc-length `dist` measured clockwise from the top-left corner. */
static GPoint point_on_perimeter(int dist, int w, int h, int inset) {
    int top_len   = w - 2 * inset;
    int right_len = h - 2 * inset;
    int perim     = 2 * (top_len + right_len);
    dist = ((dist % perim) + perim) % perim;

    if (dist < top_len)   return GPoint(inset + dist, inset);
    dist -= top_len;
    if (dist < right_len) return GPoint(w - inset, inset + dist);
    dist -= right_len;
    if (dist < top_len)   return GPoint(w - inset - dist, h - inset);
    dist -= top_len;
    return GPoint(inset, h - inset - dist);
}

static void layer_draw(Layer *layer, GContext *ctx) {
    GRect  bounds = layer_get_bounds(layer);
    GColor accent = theme_accent_color();
    int    w = bounds.size.w;
    int    h = bounds.size.h;
    int    i = FRAME_INSET;

    /* Outer rectangle border */
    graphics_context_set_stroke_color(ctx, GColorDarkGray);
    graphics_context_set_stroke_width(ctx, 1);
    graphics_draw_rect(ctx, GRect(i, i, w - 2 * i, h - 2 * i));

    /* Four L-shaped corner brackets */
    int arm = 18;
    int ci  = i + 4;
    int x0 = ci,     y0 = ci;
    int x1 = w - ci, y1 = h - ci;
    graphics_context_set_stroke_width(ctx, 4);
    /* TL */
    graphics_draw_line(ctx, GPoint(x0, y0), GPoint(x0 + arm, y0));
    graphics_draw_line(ctx, GPoint(x0, y0), GPoint(x0, y0 + arm));
    /* TR */
    graphics_draw_line(ctx, GPoint(x1, y0), GPoint(x1 - arm, y0));
    graphics_draw_line(ctx, GPoint(x1, y0), GPoint(x1, y0 + arm));
    /* BL */
    graphics_draw_line(ctx, GPoint(x0, y1), GPoint(x0 + arm, y1));
    graphics_draw_line(ctx, GPoint(x0, y1), GPoint(x0, y1 - arm));
    /* BR */
    graphics_draw_line(ctx, GPoint(x1, y1), GPoint(x1 - arm, y1));
    graphics_draw_line(ctx, GPoint(x1, y1), GPoint(x1, y1 - arm));

    /* 8 nodes evenly spaced around perimeter, offset by s_node_offset */
    int top_len = w - 2 * i;
    int perim   = 2 * (top_len + (h - 2 * i));
    int spacing = perim / 8;
    int home    = (top_len - spacing) / 2;  /* start offset so top-center aligns */

    graphics_context_set_fill_color(ctx, GColorIslamicGreen);
    for (int k = 0; k < 8; k++) {
        GPoint p = point_on_perimeter(home + s_node_offset + k * spacing, w, h, i);
        graphics_fill_circle(ctx, p, NODE_RADIUS);
    }
}

/* === Rotation timer === */

#define NUM_ROT_FRAMES 20

static void rot_timer_callback(void *context) {
    s_rot_timer = NULL;
    if (!s_layer) return;

    s_rot_frame++;
    if (s_rot_frame >= NUM_ROT_FRAMES) {
        s_node_offset = 0;
        s_rot_frame   = 0;
        layer_mark_dirty(s_layer);
        return;
    }

    GRect bounds = layer_get_bounds(s_layer);
    int w = bounds.size.w, h = bounds.size.h;
    int quarter_perim = (2 * ((w - 2 * FRAME_INSET) + (h - 2 * FRAME_INSET))) / 4;

    /* Ease-in/out: (1 - cos(frame/frames * π)) / 2 */
    int32_t angle = (int32_t)s_rot_frame * TRIG_MAX_ANGLE / (2 * NUM_ROT_FRAMES);
    int32_t eased = (TRIG_MAX_RATIO - cos_lookup(angle)) / 2;
    s_node_offset = (int)((int64_t)quarter_perim * eased / TRIG_MAX_RATIO);

    layer_mark_dirty(s_layer);
    s_rot_timer = app_timer_register(BLINK_INTERVAL_MS, rot_timer_callback, NULL);
}

/* === Public API === */

void frame_init(Layer *parent) {
    GRect bounds = layer_get_bounds(parent);
    s_layer = layer_create(bounds);
    layer_set_update_proc(s_layer, layer_draw);
    layer_add_child(parent, s_layer);
}

void frame_deinit(void) {
    if (s_rot_timer) { app_timer_cancel(s_rot_timer); s_rot_timer = NULL; }
    if (s_layer)     { layer_destroy(s_layer);        s_layer     = NULL; }
}

void frame_start_rotation(void) {
    if (s_rot_timer) app_timer_cancel(s_rot_timer);
    s_rot_frame   = 0;
    s_node_offset = 0;
    s_rot_timer = app_timer_register(BLINK_INTERVAL_MS, rot_timer_callback, NULL);
}

void frame_mark_dirty(void) {
    if (s_layer) layer_mark_dirty(s_layer);
}
