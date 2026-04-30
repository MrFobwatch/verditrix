#include "comp_scroll.h"
#include "registry.h"

#define SCROLL_ANIM_MS 250

static Layer            *s_layer        = NULL;
static GRect             s_diamond;
static int               s_out_idx      = 0;
static int               s_in_idx       = 0;
static int               s_offset       = 0;   /* w→0 during slide */
static bool              s_transitioning = false;
static Animation        *s_anim         = NULL;
static CompScrollDoneCb  s_done_cb      = NULL;

static void layer_draw(Layer *layer, GContext *ctx) {
    (void)layer;
    if (!s_transitioning) {
        registry_draw_index(s_out_idx, ctx, s_diamond);
        return;
    }

    GRect out_bounds = GRect(
        s_diamond.origin.x + s_offset - s_diamond.size.w,
        s_diamond.origin.y,
        s_diamond.size.w, s_diamond.size.h);

    GRect in_bounds = GRect(
        s_diamond.origin.x + s_offset,
        s_diamond.origin.y,
        s_diamond.size.w, s_diamond.size.h);

    registry_draw_index(s_out_idx, ctx, out_bounds);
    registry_draw_index(s_in_idx,  ctx, in_bounds);
}

static void anim_update(Animation *a, AnimationProgress p) {
    (void)a;
    /* p: 0 → ANIMATION_NORMALIZED_MAX; invert so s_offset runs w → 0 */
    s_offset = s_diamond.size.w -
               (int)((int64_t)p * s_diamond.size.w / ANIMATION_NORMALIZED_MAX);
    if (s_layer) layer_mark_dirty(s_layer);
}

static void anim_stopped(Animation *a, bool finished, void *ctx) {
    (void)ctx;
    s_anim = NULL;
    animation_destroy(a);
    if (!finished) return;

    s_out_idx       = s_in_idx;
    s_transitioning = false;
    s_offset        = 0;
    if (s_layer) layer_mark_dirty(s_layer);
    if (s_done_cb)  s_done_cb(s_out_idx);
}

static const AnimationImplementation s_anim_impl = { .update = anim_update };

void comp_scroll_init(Layer *parent, GRect diamond_bounds) {
    s_diamond       = diamond_bounds;
    s_out_idx       = 0;
    s_in_idx        = 0;
    s_offset        = 0;
    s_transitioning = false;

    GRect parent_bounds = layer_get_bounds(parent);
    s_layer = layer_create(parent_bounds);
    layer_set_update_proc(s_layer, layer_draw);
    layer_add_child(parent, s_layer);
}

void comp_scroll_deinit(void) {
    if (s_anim) { animation_unschedule(s_anim); s_anim = NULL; }
    s_done_cb       = NULL;
    s_transitioning = false;
    s_offset        = 0;
    if (s_layer) { layer_destroy(s_layer); s_layer = NULL; }
}

void comp_scroll_trigger(int from_idx, int to_idx, CompScrollDoneCb cb) {
    if (s_anim) { animation_unschedule(s_anim); s_anim = NULL; }

    s_out_idx       = from_idx;
    s_in_idx        = to_idx;
    s_offset        = s_diamond.size.w;
    s_transitioning = true;
    s_done_cb       = cb;

    s_anim = animation_create();
    animation_set_implementation(s_anim, &s_anim_impl);
    animation_set_duration(s_anim, SCROLL_ANIM_MS);
    animation_set_curve(s_anim, AnimationCurveEaseInOut);
    animation_set_handlers(s_anim,
        (AnimationHandlers){ .stopped = anim_stopped }, NULL);
    animation_schedule(s_anim);
}

void comp_scroll_mark_dirty(void) {
    if (s_layer) layer_mark_dirty(s_layer);
}
