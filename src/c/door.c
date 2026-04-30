#include "door.h"
#include "config.h"
#include "theme.h"
#include <pebble-effect-layer/pebble-effect-layer.h>

static Layer       *s_layer        = NULL;
static GRect        s_diamond;
static int          s_progress     = 0;    /* 0 = closed, 100 = open */
static bool         s_opening      = false;
static DoorState    s_state        = DOOR_CLOSED;
static Animation   *s_anim         = NULL;
static DoorStateCb  s_cb           = NULL;
static void        *s_cb_ctx       = NULL;
static DoorFrameCb  s_frame_cb     = NULL;
static void        *s_frame_cb_ctx = NULL;

/* 5-point X-half polygons. Points live in these arrays; GPath holds pointers
 * into them so mutating the arrays before each draw updates the path geometry
 * without any per-frame allocation. */
static GPoint    s_left_pts[5];
static GPoint    s_right_pts[5];
static GPathInfo s_left_info  = { 5, s_left_pts  };
static GPathInfo s_right_info = { 5, s_right_pts };
static GPath    *s_left_path  = NULL;
static GPath    *s_right_path = NULL;

static char s_hh_buf[3] = "  ";
static char s_mm_buf[3] = "  ";

/* === Geometry ===
 *
 * Each half is a 5-point polygon (indices 0..4):
 *
 *   LEFT half (wings/waist slide right as door opens):
 *     [0] top-tip      (cx,                    top)     — fixed
 *     [1] upper wing   (cx - wing_x + dx,      top)     — slides right
 *     [2] waist        (cx - waist_x + dx,     cy)      — slides right
 *     [3] lower wing   (cx - wing_x + dx,      bottom)  — slides right
 *     [4] bottom-tip   (cx,                    bottom)  — fixed
 *
 *   RIGHT half: mirror of left across cx.
 *
 * At progress=0 (dx=0): both halves form a closed X with tips meeting at cx.
 * At progress=100 (dx=cx): wings near screen edges, complication fully revealed.
 */
static void rebuild_polygons(void) {
    int hw = s_diamond.size.w / 2;
    int hh = s_diamond.size.h / 2;
    int cx = s_diamond.origin.x + hw;
    int cy = s_diamond.origin.y + hh;
    int top    = s_diamond.origin.y;
    int bottom = s_diamond.origin.y + s_diamond.size.h;

    int wing_x  = hw * X_WING_PCT  / 100;
    int waist_x = hw * X_WAIST_PCT / 100;

    int dx = (s_progress * cx) / 100;

    s_left_pts[0] = GPoint(cx, top);
    s_left_pts[1] = GPoint(cx - wing_x + dx, top);
    s_left_pts[2] = GPoint(cx - waist_x + dx, cy);
    s_left_pts[3] = GPoint(cx - wing_x + dx, bottom);
    s_left_pts[4] = GPoint(cx, bottom);

    for (int i = 0; i < 5; i++) {
        s_right_pts[i] = GPoint(2 * cx - s_left_pts[i].x, s_left_pts[i].y);
    }
}

/* === Drawing === */

/* Hours above center, minutes below — vertically stacked on the X face. */
static void draw_time(GContext *ctx, GRect bounds) {
    if (s_progress > 30) return;

    int cx = bounds.size.w / 2;
    int cy = bounds.size.h / 2;

    GFont font = fonts_get_system_font(FONT_KEY_LECO_60_BOLD_NUMBERS_AM_PM);
    graphics_context_set_text_color(ctx, OMNI_TIME_TEXT);

    graphics_draw_text(ctx, s_hh_buf, font,
                       GRect(cx - 40, cy - cy * 90 / 100, 80, 50),
                       GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
    graphics_draw_text(ctx, s_mm_buf, font,
                       GRect(cx - 40, cy + cy * 30 / 100, 80, 50),
                       GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
}

static void layer_draw(Layer *layer, GContext *ctx) {
    GRect bounds = layer_get_bounds(layer);
    rebuild_polygons();

    GColor accent = theme_accent_color();

    graphics_context_set_fill_color(ctx, accent);
    gpath_draw_filled(ctx, s_left_path);
    gpath_draw_filled(ctx, s_right_path);

    /* Pebble's GPath fill excludes the shared closing edge at x=cx from both
     * polygon fills, leaving a 1px sliver. A 1px stroke seals it. */
    int cx_seam = s_diamond.origin.x + s_diamond.size.w / 2;
    graphics_context_set_stroke_color(ctx, accent);
    graphics_context_set_stroke_width(ctx, 1);
    graphics_draw_line(ctx, GPoint(cx_seam, s_diamond.origin.y),
                            GPoint(cx_seam, s_diamond.origin.y + s_diamond.size.h - 1));

    /* Outer edges only — inner vertical edge (p[4]→p[0]) is skipped to avoid
     * drawing a bisecting line through the center of the X at rest. */
    graphics_context_set_stroke_color(ctx, GColorBlack);
    graphics_context_set_stroke_width(ctx, 8);
    for (int i = 1; i < 3; i++) {
        graphics_draw_line(ctx, s_left_pts[i],  s_left_pts[i + 1]);
        graphics_draw_line(ctx, s_right_pts[i], s_right_pts[i + 1]);
    }

    draw_time(ctx, bounds);
}

/* === Animation === */

static void anim_update(Animation *a, AnimationProgress p) {
    int t = (p * 100) / ANIMATION_NORMALIZED_MAX;
    s_progress = s_opening ? t : (100 - t);
    if (s_layer)    layer_mark_dirty(s_layer);
    if (s_frame_cb) s_frame_cb(s_progress, s_frame_cb_ctx);
}

static void anim_stopped(Animation *a, bool finished, void *ctx) {
    s_anim = NULL;
    animation_destroy(a);
    if (!finished) return;  /* interrupted by start_anim — caller handles state */
    s_state    = s_opening ? DOOR_OPEN : DOOR_CLOSED;
    s_progress = s_opening ? 100 : 0;
    if (s_layer) layer_mark_dirty(s_layer);
    if (s_cb)    s_cb(s_state, s_cb_ctx);
}

static const AnimationImplementation s_anim_impl = { .update = anim_update };

static void start_anim(bool opening) {
    if (s_anim) { animation_unschedule(s_anim); s_anim = NULL; }
    s_opening = opening;
    s_state   = opening ? DOOR_OPENING : DOOR_CLOSING;

    s_anim = animation_create();
    animation_set_implementation(s_anim, &s_anim_impl);
    animation_set_duration(s_anim, DOOR_ANIM_MS);
    animation_set_curve(s_anim, AnimationCurveEaseInOut);
    animation_set_handlers(s_anim,
        (AnimationHandlers){ .stopped = anim_stopped }, NULL);
    animation_schedule(s_anim);
}

/* === Public API === */

void door_init(Layer *parent, GRect diamond_bounds) {
    s_diamond  = diamond_bounds;
    s_progress = 0;
    s_state    = DOOR_CLOSED;

    rebuild_polygons();
    s_left_path  = gpath_create(&s_left_info);
    s_right_path = gpath_create(&s_right_info);

    GRect parent_bounds = layer_get_bounds(parent);
    s_layer = layer_create(parent_bounds);
    layer_set_update_proc(s_layer, layer_draw);
    layer_add_child(parent, s_layer);
}

void door_deinit(void) {
    if (s_anim)       { animation_unschedule(s_anim); s_anim = NULL; }
    s_cb       = NULL;
    s_frame_cb = NULL;
    if (s_left_path)  { gpath_destroy(s_left_path);  s_left_path  = NULL; }
    if (s_right_path) { gpath_destroy(s_right_path); s_right_path = NULL; }
    if (s_layer)      { layer_destroy(s_layer);      s_layer      = NULL; }
}

void door_open(void) {
    if (s_state != DOOR_OPENING && s_state != DOOR_OPEN) start_anim(true);
}

void door_close(void) {
    if (s_state != DOOR_CLOSING && s_state != DOOR_CLOSED) start_anim(false);
}

DoorState door_state(void)    { return s_state; }
int       door_progress(void) { return s_progress; }

void door_mark_dirty(void) {
    if (s_layer) layer_mark_dirty(s_layer);
}

void door_set_state_cb(DoorStateCb cb, void *ctx) {
    s_cb     = cb;
    s_cb_ctx = ctx;
}

void door_set_frame_cb(DoorFrameCb cb, void *ctx) {
    s_frame_cb     = cb;
    s_frame_cb_ctx = ctx;
}

void door_set_time(const char *hh, const char *mm) {
    if (hh) { s_hh_buf[0] = hh[0]; s_hh_buf[1] = hh[1]; s_hh_buf[2] = '\0'; }
    if (mm) { s_mm_buf[0] = mm[0]; s_mm_buf[1] = mm[1]; s_mm_buf[2] = '\0'; }
    if (s_layer) layer_mark_dirty(s_layer);
}
