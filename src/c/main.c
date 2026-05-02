/*
 * main.c — orchestration only. No drawing logic, no animation math.
 *
 * Layer stack (bottom to top):
 *
 *   Root window layer
 *    ├── s_bg_layer     solid OMNI_BG fill
 *    ├── door layer     X-half doors + time text (owned by door.c)
 *    ├── comp_scroll    animated complication layer (owned by comp_scroll.c)
 *    ├── comp_mask      static inverse diamond mask (owned by comp_mask.c)
 *    └── frame          chrome decoration — topmost, always visible (owned by frame.c)
 *
 * Sequence state machine (wraps door.c's OPENING/CLOSING animation):
 *
 *   IDLE ─tap→ BLINK_OUT ─done→ [door opens] ─done→ HOLD ─timer→
 *   WARN_WHITE ─done→ WARN ─done→ [door closes] ─done→ IDLE
 */

#include <pebble.h>

#include "config.h"
#include "theme.h"
#include "door.h"
#include "frame.h"
#include "registry.h"
#include "comp_scroll.h"
#include "comp_mask.h"


/* comp_weather_set is declared here since comp_weather.c has no public header */
void comp_weather_set(int temp, const char *cond);

static Window *s_window;
static Layer  *s_bg_layer;
static GRect   s_diamond_bounds;

/* ============================================================
 * High-level sequence state machine.
 * ============================================================ */
typedef enum {
    SEQ_IDLE,
    SEQ_BLINK_OUT,
    SEQ_HOLD,
    SEQ_WARN_WHITE,
    SEQ_WARN,
} SeqState;

static SeqState  s_seq       = SEQ_IDLE;
static int       s_blink_frame = 0;
static AppTimer *s_seq_timer  = NULL;

static bool s_rotate_on_open = true;  /* initial CW rotation when door opens */

static void cancel_seq_timer(void) {
    if (s_seq_timer) {
        app_timer_cancel(s_seq_timer);
        s_seq_timer = NULL;
    }
}

static void mark_all_dirty(void) {
    door_mark_dirty();
    frame_mark_dirty();
    comp_scroll_mark_dirty();
}

static void seq_callback(void *context);  /* forward */

static void schedule_seq(uint32_t ms) {
    s_seq_timer = app_timer_register(ms, seq_callback, NULL);
}

static void seq_callback(void *context) {
    s_seq_timer = NULL;
    s_blink_frame++;

    switch (s_seq) {
        case SEQ_BLINK_OUT:
            /* Alternate green ↔ black; after NUM_BLINK_FRAMES, open doors */
            theme_set_accent((s_blink_frame % 2 == 0) ? GColorMalachite : GColorBlack);
            mark_all_dirty();
            if (s_blink_frame >= NUM_BLINK_FRAMES) {
                theme_set_accent(GColorMalachite);
                door_open();
                return;  /* door's anim_stopped fires on_door_state_change */
            }
            schedule_seq(BLINK_INTERVAL_MS);
            break;

        case SEQ_HOLD:
            /* Timer expired — flash white for 1s then enter WARN */
            s_seq = SEQ_WARN_WHITE;
            s_blink_frame = 0;
            theme_set_accent(GColorWhite);
            mark_all_dirty();
            schedule_seq(1000);
            break;

        case SEQ_WARN_WHITE:
            /* White phase done — start red blinking */
            s_seq = SEQ_WARN;
            s_blink_frame = 0;
            theme_set_accent(GColorRed);
            mark_all_dirty();
            schedule_seq(BLINK_INTERVAL_MS);
            break;

        case SEQ_WARN:
            /* Alternate red ↔ black; interval grows each frame so blink trails off */
            theme_set_accent((s_blink_frame % 2 == 0) ? GColorRed : GColorBlack);
            mark_all_dirty();
            if (s_blink_frame >= NUM_WARN_FRAMES) {
                comp_mask_set_visible(false);
                door_close();
                return;
            }
            schedule_seq(BLINK_INTERVAL_MS + s_blink_frame * WARN_STEP_MS);
            break;

        default:
            break;
    }
}

/* ============================================================
 * Background layer.
 * ============================================================ */
static void bg_draw(Layer *l, GContext *ctx) {
    graphics_context_set_fill_color(ctx, OMNI_BG);
    graphics_fill_rect(ctx, layer_get_bounds(l), 0, GCornerNone);
}

static void on_scroll_done(int new_idx) {
    registry_set_current(new_idx);
    frame_rotate_cw();
}

/* ============================================================
 * Door state callback — fires when door reaches a stable state.
 * ============================================================ */
static void on_door_state_change(DoorState state, void *ctx) {
    switch (state) {
        case DOOR_OPEN:
            comp_scroll_set_exit_width(-1);
            comp_mask_set_split(0);
            comp_scroll_set_visible(true);
            comp_mask_set_visible(true);
            if (s_rotate_on_open) frame_rotate_cw();
            cancel_seq_timer();
            s_seq = SEQ_HOLD;
            s_blink_frame = 0;
            schedule_seq(HOLD_DURATION_MS);
            break;
        case DOOR_CLOSED:
            comp_scroll_set_exit_width(-1);
            comp_scroll_set_visible(false);
            comp_mask_set_visible(false);
            cancel_seq_timer();
            s_seq = SEQ_IDLE;
            theme_set_accent(GColorMalachite);
            frame_rotate_ccw();
            break;
        default:
            break;
    }
    comp_scroll_mark_dirty();
}

/* ============================================================
 * Door frame callback — keeps complication in sync with animation.
 * ============================================================ */
static void on_door_frame(int progress, void *ctx) {
    if (door_state() == DOOR_CLOSING) {
        int cur_w = s_diamond_bounds.size.w * progress / 100;
        comp_scroll_set_exit_width(cur_w);
    }
    comp_scroll_mark_dirty();
}

/* ============================================================
 * Tap handler.
 * ============================================================ */
static void on_tap(AccelAxisType axis, int32_t direction) {
    DoorState st = door_state();

    if (st == DOOR_CLOSED || st == DOOR_CLOSING) {
        if (door_state() == DOOR_CLOSING) return;  /* ignore during close animation */
        cancel_seq_timer();
        s_seq = SEQ_BLINK_OUT;
        s_blink_frame = 0;
        theme_set_accent(GColorMalachite);
        schedule_seq(BLINK_INTERVAL_MS);
    } else {
        /* Already open — trigger scroll to next complication */
        int from = registry_current_idx();
        int to   = registry_peek_next();
        comp_scroll_trigger(from, to, on_scroll_done);
        if (s_seq == SEQ_HOLD || s_seq == SEQ_WARN || s_seq == SEQ_WARN_WHITE) {
            cancel_seq_timer();
            s_seq = SEQ_HOLD;
            s_blink_frame = 0;
            theme_set_accent(GColorMalachite);
            mark_all_dirty();
            schedule_seq(TAP_HOLD_DURATION_MS);
        }
    }
}

/* ============================================================
 * Time tick.
 * ============================================================ */
static void on_tick(struct tm *t, TimeUnits changed) {
    static char hh_buf[3];
    static char mm_buf[3];
    strftime(hh_buf, sizeof(hh_buf), clock_is_24h_style() ? "%H" : "%I", t);
    strftime(mm_buf, sizeof(mm_buf), "%M", t);
    door_set_time(hh_buf, mm_buf);
    registry_tick(t);
    comp_scroll_mark_dirty();

    /* Request weather every 30 minutes */
    if (t->tm_min % 30 == 0) {
        DictionaryIterator *iter;
        if (app_message_outbox_begin(&iter) == APP_MSG_OK) {
            dict_write_uint8(iter, MESSAGE_KEY_REQUEST_WEATHER, 1);
            app_message_outbox_send();
        }
    }
}

/* ============================================================
 * AppMessage — weather data in from phone.
 * ============================================================ */
static void inbox_received(DictionaryIterator *iter, void *ctx) {
    Tuple *temp_t = dict_find(iter, MESSAGE_KEY_TEMPERATURE);
    Tuple *cond_t = dict_find(iter, MESSAGE_KEY_CONDITIONS);
    if (temp_t || cond_t) {
        /* Pass INT32_MIN for temp when absent so comp_weather_set leaves it unchanged */
        comp_weather_set(
            temp_t ? (int)temp_t->value->int32 : INT32_MIN,
            cond_t ? cond_t->value->cstring    : NULL
        );
        comp_scroll_mark_dirty();
    }
}

/* ============================================================
 * Window lifecycle.
 * ============================================================ */
static void window_load(Window *window) {
    Layer *root   = window_get_root_layer(window);
    GRect  bounds = layer_get_bounds(root);

    int diamond_size_w = bounds.size.w - DIAMOND_INSET;
    int diamond_size_h = bounds.size.h - DIAMOND_INSET_H;

    s_diamond_bounds = GRect(
        (bounds.size.w  - diamond_size_w) / 2,
        (bounds.size.h  - diamond_size_h) / 2,
        diamond_size_w,
        diamond_size_h
    );

    registry_init();
    time_t now = time(NULL);
    on_tick(localtime(&now), MINUTE_UNIT);

    s_bg_layer = layer_create(bounds);
    layer_set_update_proc(s_bg_layer, bg_draw);
    layer_add_child(root, s_bg_layer);

    door_init(root, s_diamond_bounds);

    comp_scroll_init(root, s_diamond_bounds);
    comp_mask_init(root, bounds);
    comp_scroll_set_visible(false);
    comp_mask_set_visible(false);

    frame_init(root);  /* topmost — always visible above mask and door */

    door_set_state_cb(on_door_state_change, NULL);
    door_set_frame_cb(on_door_frame, NULL);

    // frame_start_rotation();
}

static void window_unload(Window *window) {
    cancel_seq_timer();
    comp_mask_deinit();
    comp_scroll_deinit();
    door_deinit();
    frame_deinit();
    if (s_bg_layer)   { layer_destroy(s_bg_layer);   s_bg_layer   = NULL; }
    registry_deinit();
}

/* ============================================================
 * App entry.
 * ============================================================ */
static void init(void) {
    s_window = window_create();
    window_set_background_color(s_window, OMNI_BG);
    window_set_window_handlers(s_window, (WindowHandlers){
        .load   = window_load,
        .unload = window_unload,
    });
    window_stack_push(s_window, true);

    tick_timer_service_subscribe(MINUTE_UNIT, on_tick);
    accel_tap_service_subscribe(on_tap);

    app_message_register_inbox_received(inbox_received);
    app_message_open(128, 128);
}

static void deinit(void) {
    accel_tap_service_unsubscribe();
    tick_timer_service_unsubscribe();
    if (s_window) { window_destroy(s_window); s_window = NULL; }
}

int main(void) {
    init();
    app_event_loop();
    deinit();
    return 0;
}
