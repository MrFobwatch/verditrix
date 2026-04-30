# Complication Scroll Transition + Dynamic Resolution — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

> **⚠️ Emulator / device testing:** Do NOT launch the Pebble emulator or install to a device. Build verification (`pebble build`) is sufficient per task. The human handles all emulator runs — they are fragile in this environment.

**Goal:** Add a horizontal slide animation between complications (masked to the diamond reveal area) and replace all hardcoded pixel values in complication draw functions with resolution-proportional calculations.

**Architecture:** Two new modules — `comp_mask` (static inverse mask over non-diamond corners) and `comp_scroll` (animated dual-slot complication layer) — replace the existing `s_comp_layer` in `main.c`. The door layer remains unchanged on top. All complication draw functions derive layout from a `u = MIN(w,h)/10` base unit computed from the `bounds` argument.

**Tech Stack:** Pebble C SDK, waf build system (`pebble build`), `GPath`/`GPathInfo` for mask triangles, `AnimationImplementation` for the slide.

---

## File Map

| File | Action | Responsibility |
|------|--------|----------------|
| `src/c/comp_mask.h` | Create | Public API for the static inverse mask layer |
| `src/c/comp_mask.c` | Create | Draws 4 corner triangles in OMNI_BG over non-diamond area |
| `src/c/comp_scroll.h` | Create | Public API + `CompScrollDoneCb` typedef |
| `src/c/comp_scroll.c` | Create | Two-slot complication layer with horizontal slide animation |
| `src/c/registry.h` | Modify | Declare `registry_draw_index`, `registry_set_current`, `registry_current_idx`, `registry_peek_next` |
| `src/c/registry.c` | Modify | Implement the four new functions; update `registry_draw` to delegate |
| `src/c/main.c` | Modify | Replace `s_comp_layer` with scroll+mask, update tap handler, add `on_scroll_done` |
| `src/c/comp_weather.c` | Modify | Replace hardcoded pixels with `u`-based proportional layout |
| `src/c/comp_date.c` | Modify | Replace hardcoded pixels with `u`-based proportional layout |
| `src/c/comp_battery.c` | Modify | Replace hardcoded pixels with `u`-based proportional layout |

> **No wscript changes needed.** The build uses `ant_glob('src/c/**/*.c')` — every `.c` file in `src/c/` is compiled automatically.

---

## Task 1: Registry extensions

**Files:**
- Modify: `src/c/registry.h`
- Modify: `src/c/registry.c`

- [ ] **Step 1: Add declarations to `registry.h`**

Replace the existing content of `src/c/registry.h` with:

```c
#pragma once
#include "complication.h"

void registry_init(void);
void registry_deinit(void);

/* Cycle to next complication. Caller is responsible for triggering redraw. */
void registry_next(void);

/* Forward tick events to all complications. */
void registry_tick(struct tm *t);

/* Render active complication into the given bounds. */
void registry_draw(GContext *ctx, GRect bounds);

/* Render a specific complication by index. idx is clamped to [0, N-1]. */
void registry_draw_index(int idx, GContext *ctx, GRect bounds);

/* Set the active complication index directly (called by scroll done cb). */
void registry_set_current(int idx);

/* Return the current active index. */
int registry_current_idx(void);

/* Return what the next index would be after cycling (does not change state). */
int registry_peek_next(void);

/* For diagnostics */
const char *registry_current_name(void);
```

- [ ] **Step 2: Add the four new functions to `registry.c`**

Open `src/c/registry.c`. After the existing `registry_draw` function, add:

```c
void registry_draw_index(int idx, GContext *ctx, GRect bounds) {
    if (idx < 0 || idx >= (int)N_COMPS) idx = 0;
    complication_draw(*s_comps[idx], ctx, bounds);
}

void registry_set_current(int idx) {
    s_current = ((idx % (int)N_COMPS) + (int)N_COMPS) % (int)N_COMPS;
}

int registry_current_idx(void) {
    return s_current;
}

int registry_peek_next(void) {
    return (s_current + 1) % (int)N_COMPS;
}
```

Also update `registry_draw` to delegate to `registry_draw_index`:

```c
void registry_draw(GContext *ctx, GRect bounds) {
    registry_draw_index(s_current, ctx, bounds);
}
```

- [ ] **Step 3: Build to verify no errors**

```bash
cd /home/kevin/claudetrix && pebble build
```

Expected: build completes with no errors. Warnings about unused functions are acceptable.

- [ ] **Step 4: Commit**

```bash
cd /home/kevin/claudetrix
git add src/c/registry.h src/c/registry.c
git commit -m "feat: add registry_draw_index, set_current, current_idx, peek_next"
```

---

## Task 2: comp_mask module

**Files:**
- Create: `src/c/comp_mask.h`
- Create: `src/c/comp_mask.c`

- [ ] **Step 1: Create `src/c/comp_mask.h`**

```c
#pragma once
#include <pebble.h>

/*
 * comp_mask — static inverse mask layer.
 *
 * Draws four filled triangles in OMNI_BG over the non-diamond corner areas,
 * so complication content drawn outside the diamond rhombus is hidden
 * regardless of what the complication renders.
 *
 * Layer is added as a child of parent and sits above the comp_scroll layer
 * but below the door layer. It never needs to be invalidated after init.
 */

void comp_mask_init(Layer *parent, GRect diamond_bounds);
void comp_mask_deinit(void);
```

- [ ] **Step 2: Create `src/c/comp_mask.c`**

```c
#include "comp_mask.h"
#include "config.h"

static Layer    *s_layer     = NULL;
static GPoint    s_pts[4][3];
static GPathInfo s_infos[4]  = {
    { 3, s_pts[0] },
    { 3, s_pts[1] },
    { 3, s_pts[2] },
    { 3, s_pts[3] },
};
static GPath    *s_paths[4]  = { NULL, NULL, NULL, NULL };

static void layer_draw(Layer *layer, GContext *ctx) {
    graphics_context_set_fill_color(ctx, OMNI_BG);
    for (int i = 0; i < 4; i++) {
        gpath_draw_filled(ctx, s_paths[i]);
    }
}

void comp_mask_init(Layer *parent, GRect d) {
    int cx = d.origin.x + d.size.w / 2;
    int cy = d.origin.y + d.size.h / 2;
    int x0 = d.origin.x;
    int y0 = d.origin.y;
    int x1 = d.origin.x + d.size.w;
    int y1 = d.origin.y + d.size.h;

    /* top-left triangle */
    s_pts[0][0] = GPoint(x0, y0);
    s_pts[0][1] = GPoint(cx, y0);
    s_pts[0][2] = GPoint(x0, cy);

    /* top-right triangle */
    s_pts[1][0] = GPoint(cx, y0);
    s_pts[1][1] = GPoint(x1, y0);
    s_pts[1][2] = GPoint(x1, cy);

    /* bottom-right triangle */
    s_pts[2][0] = GPoint(x1, cy);
    s_pts[2][1] = GPoint(x1, y1);
    s_pts[2][2] = GPoint(cx, y1);

    /* bottom-left triangle */
    s_pts[3][0] = GPoint(x0, cy);
    s_pts[3][1] = GPoint(cx, y1);
    s_pts[3][2] = GPoint(x0, y1);

    for (int i = 0; i < 4; i++) {
        s_paths[i] = gpath_create(&s_infos[i]);
    }

    GRect parent_bounds = layer_get_bounds(parent);
    s_layer = layer_create(parent_bounds);
    layer_set_update_proc(s_layer, layer_draw);
    layer_add_child(parent, s_layer);
}

void comp_mask_deinit(void) {
    for (int i = 0; i < 4; i++) {
        if (s_paths[i]) { gpath_destroy(s_paths[i]); s_paths[i] = NULL; }
    }
    if (s_layer) { layer_destroy(s_layer); s_layer = NULL; }
}
```

- [ ] **Step 3: Build to verify no errors**

```bash
cd /home/kevin/claudetrix && pebble build
```

Expected: build completes with no errors. `comp_mask_init`/`comp_mask_deinit` are defined but not yet called — that is fine.

- [ ] **Step 4: Commit**

```bash
cd /home/kevin/claudetrix
git add src/c/comp_mask.h src/c/comp_mask.c
git commit -m "feat: add comp_mask static inverse diamond mask layer"
```

---

## Task 3: comp_scroll module

**Files:**
- Create: `src/c/comp_scroll.h`
- Create: `src/c/comp_scroll.c`

- [ ] **Step 1: Create `src/c/comp_scroll.h`**

```c
#pragma once
#include <pebble.h>

/*
 * comp_scroll — animated complication scroll layer.
 *
 * Owns two slots: outgoing and incoming. When comp_scroll_trigger is called,
 * the outgoing complication slides left and the incoming slides in from the
 * right. The comp_mask layer above provides diagonal masking; this layer
 * draws freely across its full bounds.
 *
 * The done callback fires (with the new index) only when the animation
 * completes naturally. If a new trigger interrupts an in-progress animation,
 * the done callback for the interrupted animation is silently dropped.
 */

typedef void (*CompScrollDoneCb)(int new_idx);

void comp_scroll_init(Layer *parent, GRect diamond_bounds);
void comp_scroll_deinit(void);

/*
 * Start a left-slide from from_idx to to_idx.
 * Safe to call during a transition — the current animation is cancelled and
 * a fresh one starts from from_idx.
 */
void comp_scroll_trigger(int from_idx, int to_idx, CompScrollDoneCb cb);

void comp_scroll_mark_dirty(void);
```

- [ ] **Step 2: Create `src/c/comp_scroll.c`**

```c
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
    s_done_cb = NULL;
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
```

- [ ] **Step 3: Build to verify no errors**

```bash
cd /home/kevin/claudetrix && pebble build
```

Expected: build completes with no errors.

- [ ] **Step 4: Commit**

```bash
cd /home/kevin/claudetrix
git add src/c/comp_scroll.h src/c/comp_scroll.c
git commit -m "feat: add comp_scroll horizontal slide animation layer"
```

---

## Task 4: Integrate comp_scroll and comp_mask into main.c

**Files:**
- Modify: `src/c/main.c`

This task removes `s_comp_layer` and `comp_draw`, wires in the two new modules, and updates the tap handler to trigger a slide instead of an instant switch.

- [ ] **Step 1: Update includes and remove `s_comp_layer`**

At the top of `main.c`, add two includes after the existing ones:

```c
#include "comp_scroll.h"
#include "comp_mask.h"
```

Remove this line from the static variable declarations:

```c
static Layer  *s_comp_layer;
```

- [ ] **Step 2: Remove `comp_draw` and update `mark_all_dirty`**

Remove the entire `comp_draw` function:

```c
/* REMOVE this block entirely: */
static void comp_draw(Layer *l, GContext *ctx) {
    if (door_progress() < 5) return;
    registry_draw(ctx, s_diamond_bounds);
}
```

Replace `mark_all_dirty` with:

```c
static void mark_all_dirty(void) {
    door_mark_dirty();
    frame_mark_dirty();
    comp_scroll_mark_dirty();
}
```

- [ ] **Step 3: Add `on_scroll_done` callback (before `on_door_state_change`)**

Insert this function before `on_door_state_change`:

```c
static void on_scroll_done(int new_idx) {
    registry_set_current(new_idx);
}
```

- [ ] **Step 4: Update `on_door_state_change` and `on_door_frame`**

Replace `layer_mark_dirty(s_comp_layer)` with `comp_scroll_mark_dirty()` in both:

In `on_door_state_change` (last line before closing brace):
```c
    comp_scroll_mark_dirty();
```

In `on_door_frame`:
```c
static void on_door_frame(int progress, void *ctx) {
    comp_scroll_mark_dirty();
}
```

- [ ] **Step 5: Update `on_tap` — door-open branch**

Replace the `else` branch in `on_tap`:

```c
    } else {
        /* Already open — trigger scroll to next complication */
        int from = registry_current_idx();
        int to   = registry_peek_next();
        comp_scroll_trigger(from, to, on_scroll_done);
        if (s_seq == SEQ_HOLD || s_seq == SEQ_WARN) {
            cancel_seq_timer();
            s_seq = SEQ_HOLD;
            s_blink_frame = 0;
            theme_set_accent(GColorMalachite);
            mark_all_dirty();
            schedule_seq(HOLD_DURATION_MS);
        }
    }
```

- [ ] **Step 6: Update `on_tick` and `inbox_received`**

In `on_tick`, replace `layer_mark_dirty(s_comp_layer)` with:
```c
    comp_scroll_mark_dirty();
```

In `inbox_received`, replace `layer_mark_dirty(s_comp_layer)` with:
```c
        comp_scroll_mark_dirty();
```

- [ ] **Step 7: Update `window_load`**

Replace the comp_layer creation block:
```c
    /* REMOVE: */
    s_comp_layer = layer_create(bounds);
    layer_set_update_proc(s_comp_layer, comp_draw);
    layer_add_child(root, s_comp_layer);

    /* REPLACE WITH (insert after frame_init, before door_init): */
    comp_scroll_init(root, s_diamond_bounds);
    comp_mask_init(root, s_diamond_bounds);
```

The resulting `window_load` layer-setup section should read:

```c
    s_bg_layer = layer_create(bounds);
    layer_set_update_proc(s_bg_layer, bg_draw);
    layer_add_child(root, s_bg_layer);

    frame_init(root);

    comp_scroll_init(root, s_diamond_bounds);
    comp_mask_init(root, s_diamond_bounds);

    door_init(root, s_diamond_bounds);
    door_set_state_cb(on_door_state_change, NULL);
    door_set_frame_cb(on_door_frame, NULL);
```

- [ ] **Step 8: Update `window_unload`**

Replace the comp_layer destroy line:
```c
    /* REMOVE: */
    if (s_comp_layer) { layer_destroy(s_comp_layer); s_comp_layer = NULL; }

    /* REPLACE WITH: */
    comp_mask_deinit();
    comp_scroll_deinit();
```

The resulting `window_unload` should read:

```c
static void window_unload(Window *window) {
    cancel_seq_timer();
    door_deinit();
    comp_mask_deinit();
    comp_scroll_deinit();
    frame_deinit();
    if (s_bg_layer) { layer_destroy(s_bg_layer); s_bg_layer = NULL; }
    registry_deinit();
}
```

- [ ] **Step 9: Build to verify no errors**

```bash
cd /home/kevin/claudetrix && pebble build
```

Expected: build completes with no errors. At this point the watchface is fully functional with the scroll animation — but still uses hardcoded pixel values in the complication draw functions.

- [ ] **Step 10: Commit**

```bash
cd /home/kevin/claudetrix
git add src/c/main.c
git commit -m "feat: wire comp_scroll and comp_mask into main, replace s_comp_layer"
```

---

## Task 5: Dynamic resolution in comp_weather.c

**Files:**
- Modify: `src/c/comp_weather.c`

- [ ] **Step 1: Replace `w_draw` with proportional layout**

Replace the entire `w_draw` function:

```c
static void w_draw(Complication *base, GContext *ctx, GRect bounds) {
    WeatherComplication *self = (WeatherComplication *)base;

    int cx = bounds.origin.x + bounds.size.w / 2;
    int cy = bounds.origin.y + bounds.size.h / 2;
    int u  = (bounds.size.w < bounds.size.h ? bounds.size.w : bounds.size.h) / 10;

    char temp_str[8];
    snprintf(temp_str, sizeof(temp_str), "%d\xc2\xb0", self->temp);

    GFont temp_font = fonts_get_system_font(FONT_KEY_LECO_28_LIGHT_NUMBERS);
    GFont cond_font = fonts_get_system_font(FONT_KEY_ROBOTO_CONDENSED_21);

    graphics_context_set_text_color(ctx, OMNI_COMP_TEXT);

    GRect temp_rect = GRect(cx - u * 3,     cy - u * 2,     u * 6, u * 3);
    GRect cond_rect = GRect(cx - u * 7 / 2, cy + u / 2,     u * 7, u * 2);

    graphics_draw_text(ctx, temp_str, temp_font, temp_rect,
                       GTextOverflowModeFill, GTextAlignmentCenter, NULL);
    if (self->conditions[0]) {
        graphics_draw_text(ctx, self->conditions, cond_font, cond_rect,
                           GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    }
}
```

- [ ] **Step 2: Build**

```bash
cd /home/kevin/claudetrix && pebble build
```

Expected: no errors.

- [ ] **Step 3: Commit**

```bash
cd /home/kevin/claudetrix
git add src/c/comp_weather.c
git commit -m "feat: dynamic resolution layout in comp_weather"
```

---

## Task 6: Dynamic resolution in comp_date.c

**Files:**
- Modify: `src/c/comp_date.c`

- [ ] **Step 1: Replace `d_draw` with proportional layout**

Replace the entire `d_draw` function:

```c
static void d_draw(Complication *base, GContext *ctx, GRect bounds) {
    DateComplication *self = (DateComplication *)base;

    int cx = bounds.origin.x + bounds.size.w / 2;
    int cy = bounds.origin.y + bounds.size.h / 2;
    int u  = (bounds.size.w < bounds.size.h ? bounds.size.w : bounds.size.h) / 10;

    GFont big   = fonts_get_system_font(FONT_KEY_LECO_28_LIGHT_NUMBERS);
    GFont small = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);

    graphics_context_set_text_color(ctx, OMNI_COMP_TEXT);

    GRect day_rect  = GRect(cx - bounds.size.w * 5 / 12,
                            cy - u * 3,
                            bounds.size.w * 5 / 6, u * 2);
    GRect date_rect = GRect(cx - bounds.size.w / 2,
                            cy - u / 3,
                            bounds.size.w, u * 3);

    graphics_draw_text(ctx, self->day_buf, small, day_rect,
                       GTextOverflowModeFill, GTextAlignmentCenter, NULL);
    graphics_draw_text(ctx, self->date_buf, big, date_rect,
                       GTextOverflowModeFill, GTextAlignmentCenter, NULL);
}
```

- [ ] **Step 2: Build**

```bash
cd /home/kevin/claudetrix && pebble build
```

Expected: no errors.

- [ ] **Step 3: Commit**

```bash
cd /home/kevin/claudetrix
git add src/c/comp_date.c
git commit -m "feat: dynamic resolution layout in comp_date"
```

---

## Task 7: Dynamic resolution in comp_battery.c

**Files:**
- Modify: `src/c/comp_battery.c`

- [ ] **Step 1: Replace `b_draw` with proportional layout**

Replace the entire `b_draw` function:

```c
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

    GFont font = fonts_get_system_font(FONT_KEY_LECO_36_BOLD_NUMBERS);
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
```

- [ ] **Step 2: Build**

```bash
cd /home/kevin/claudetrix && pebble build
```

Expected: no errors.

- [ ] **Step 3: Commit**

```bash
cd /home/kevin/claudetrix
git add src/c/comp_battery.c
git commit -m "feat: dynamic resolution layout in comp_battery"
```

---

## Done

All tasks complete. Hand the `.pbw` from `build/claudetrix.pbw` to the human for emulator/device testing. Key things to verify visually:

1. Tapping while door is open triggers a left-to-right slide between complications
2. Complication content clips cleanly at the diamond diagonal edges (mask working)
3. Rapid taps don't crash — they restart the slide from the original complication
4. All three complications look proportional on both basalt (144×168) and chalk (180×180) platforms
5. Door open/close animation is completely unaffected
