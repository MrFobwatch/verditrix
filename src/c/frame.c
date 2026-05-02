#include "frame.h"
#include "config.h"
#include "theme.h"

static Layer    *s_layer      = NULL;
static AppTimer *s_rot_timer  = NULL;

static int  s_rot_frame   = 0;
static int  s_rot_total   = 0;
static int  s_node_offset = 0;
static int  s_offset_from = 0;
static int  s_offset_to   = 0;
static int  s_accumulated = 0;

static int  s_spacing  = 0;
static int  s_node_base[8];
static int  s_w        = 0;
static int  s_h        = 0;

/* === Geometry helpers === */

#ifdef PBL_ROUND
static GPoint node_point_round(int32_t angle) {
    int r  = (s_w < s_h ? s_w : s_h) / 2 - FRAME_INSET_ROUND;
    int cx = s_w / 2;
    int cy = s_h / 2;
    return GPoint(cx + (int32_t)r * sin_lookup(angle) / TRIG_MAX_RATIO,
                  cy - (int32_t)r * cos_lookup(angle) / TRIG_MAX_RATIO);
}
#else
static GPoint point_on_perimeter(int dist, int w, int h, int inset) {
    int top_len   = w - 2 * inset;
    int right_len = h - 2 * inset;
    int perim     = 2 * (top_len + right_len);
    dist = ((dist % perim) + perim) % perim;

    if (dist < top_len)   return GPoint(inset + dist, inset);
    dist -= top_len;
    if (dist < right_len) return GPoint(w - inset - 1, inset + dist);
    dist -= right_len;
    if (dist < top_len)   return GPoint(w - inset - 1 - dist, h - inset - 1);
    dist -= top_len;
    return GPoint(inset, h - inset - 1 - dist);
}
#endif

/* === Drawing === */

static void layer_draw(Layer *layer, GContext *ctx) {
#ifdef PBL_ROUND
    int r  = (s_w < s_h ? s_w : s_h) / 2 - FRAME_INSET_ROUND;
    int cx = s_w / 2;
    int cy = s_h / 2;
    GRect arc_rect = GRect(cx - r, cy - r, 2 * r, 2 * r);

    /* Outer circle border */
    graphics_context_set_stroke_color(ctx, GColorDarkGray);
    graphics_context_set_stroke_width(ctx, 1);
    graphics_draw_circle(ctx, GPoint(cx, cy), r);

    /* Four arc decorations rotated 45° off cardinal (at NE/SE/SW/NW diagonals) */
    int32_t arc_half = TRIG_MAX_ANGLE * 15 / 360;  /* ±15° per decoration */
    graphics_context_set_stroke_width(ctx, 7);
    for (int q = 0; q < 4; q++) {
        int32_t center = TRIG_MAX_ANGLE / 8 + (int32_t)q * (TRIG_MAX_ANGLE / 4);
        graphics_draw_arc(ctx, arc_rect, GOvalScaleModeFitCircle,
                          center - arc_half, center + arc_half);
    }

    /* 4 nodes — fill then outline */
    graphics_context_set_fill_color(ctx, GColorIslamicGreen);
    for (int k = 0; k < 4; k++) {
        GPoint p = node_point_round(s_node_base[k] + s_node_offset);
        graphics_fill_circle(ctx, p, NODE_RADIUS);
    }
    graphics_context_set_stroke_color(ctx, GColorDarkGray);
    graphics_context_set_stroke_width(ctx, 1);
    for (int k = 0; k < 4; k++) {
        GPoint p = node_point_round(s_node_base[k] + s_node_offset);
        graphics_draw_circle(ctx, p, NODE_RADIUS);
    }
#else
    int i = FRAME_INSET;

    /* Outer rectangle border */
    graphics_context_set_stroke_color(ctx, GColorDarkGray);
    graphics_context_set_stroke_width(ctx, 1);
    graphics_draw_rect(ctx, GRect(i, i, s_w - 2 * i, s_h - 2 * i));

    /* Four L-shaped corner brackets */
    int arm = 18;
    int ci  = i + 4;
    int x0 = ci,           y0 = ci;
    int x1 = s_w - ci - 1, y1 = s_h - ci - 1;
    graphics_context_set_stroke_color(ctx, GColorDarkGray);
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

    /* 8 nodes — fill then outline */
    graphics_context_set_fill_color(ctx, GColorIslamicGreen);
    for (int k = 0; k < 8; k++) {
        GPoint p = point_on_perimeter(s_node_base[k] + s_node_offset,
                                      s_w, s_h, FRAME_INSET);
        graphics_fill_circle(ctx, p, NODE_RADIUS);
    }
    graphics_context_set_stroke_color(ctx, GColorDarkGray);
    graphics_context_set_stroke_width(ctx, 1);
    for (int k = 0; k < 8; k++) {
        GPoint p = point_on_perimeter(s_node_base[k] + s_node_offset,
                                      s_w, s_h, FRAME_INSET);
        graphics_draw_circle(ctx, p, NODE_RADIUS);
    }
#endif
}

/* === Rotation === */

static void rot_timer_callback(void *context);

static void start_rotation_to(int target, int frames) {
    if (s_rot_timer) { app_timer_cancel(s_rot_timer); s_rot_timer = NULL; }
    s_offset_from = s_node_offset;
    s_offset_to   = target;
    s_rot_frame   = 0;
    s_rot_total   = frames;
    s_rot_timer   = app_timer_register(ROT_FRAME_MS, rot_timer_callback, NULL);
}

static void rot_timer_callback(void *context) {
    s_rot_timer = NULL;
    if (!s_layer) return;

    s_rot_frame++;

    if (s_rot_frame >= s_rot_total) {
        s_node_offset = s_offset_to;
        layer_mark_dirty(s_layer);
        return;
    }

    int32_t angle = (int32_t)s_rot_frame * TRIG_MAX_ANGLE / (2 * s_rot_total);
    int32_t eased = (TRIG_MAX_RATIO - cos_lookup(angle)) / 2;
    int     delta = s_offset_to - s_offset_from;
    s_node_offset = s_offset_from + (int)((int64_t)delta * eased / TRIG_MAX_RATIO);

    layer_mark_dirty(s_layer);
    s_rot_timer = app_timer_register(ROT_FRAME_MS, rot_timer_callback, NULL);
}

void frame_rotate_cw(void) {
    s_accumulated++;
    start_rotation_to(s_accumulated * s_spacing, ROT_CW_FRAMES);
}

void frame_rotate_ccw(void) {
    if (s_accumulated <= 0) return;
    int frames = s_accumulated * ROT_CW_FRAMES;
    s_accumulated = 0;
    start_rotation_to(0, frames);
}

/* === Lifecycle === */

void frame_init(Layer *parent) {
    GRect bounds = layer_get_bounds(parent);
    s_w = bounds.size.w;
    s_h = bounds.size.h;

#ifdef PBL_ROUND
    /* 4 nodes at cardinal points; each CW notch is 1/4 rotation */
    s_spacing = TRIG_MAX_ANGLE / 4;
    for (int k = 0; k < 4; k++) {
        s_node_base[k] = k * s_spacing;
    }
#else
    int top_len   = s_w - 2 * FRAME_INSET;
    int right_len = s_h - 2 * FRAME_INSET;
    s_spacing = 2 * (top_len + right_len) / 8;

    /* 2 nodes per side at 1/3 and 2/3 of each side length */
    int tl = top_len, rl = right_len;
    s_node_base[0] = tl / 3;
    s_node_base[1] = 2 * tl / 3;
    s_node_base[2] = tl + rl / 3;
    s_node_base[3] = tl + 2 * rl / 3;
    s_node_base[4] = tl + rl + tl / 3;
    s_node_base[5] = tl + rl + 2 * tl / 3;
    s_node_base[6] = 2 * tl + rl + rl / 3;
    s_node_base[7] = 2 * tl + rl + 2 * rl / 3;
#endif

    s_layer = layer_create(bounds);
    layer_set_update_proc(s_layer, layer_draw);
    layer_add_child(parent, s_layer);
}

void frame_deinit(void) {
    if (s_rot_timer) { app_timer_cancel(s_rot_timer); s_rot_timer = NULL; }
    if (s_layer)     { layer_destroy(s_layer);        s_layer     = NULL; }
}

void frame_mark_dirty(void) {
    if (s_layer) layer_mark_dirty(s_layer);
}
