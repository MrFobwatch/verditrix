# Complication Scroll Transition + Dynamic Resolution

**Date:** 2026-04-29  
**Status:** Approved for implementation

---

## Overview

Two related improvements to the Claudetrix watchface:

1. **Complication scroll transition** — when the user taps to cycle complications while the door is open, the outgoing complication slides left and the incoming one slides in from the right, clipped to the diamond reveal area.
2. **Dynamic resolution** — all pixel values in complication draw functions are replaced with proportional calculations derived from the `bounds` parameter, so the same code renders correctly on all Pebble hardware (basalt 144×168, chalk 180×180, emery 200×228).

---

## Layer Stack (revised)

```
Root window layer
 ├── s_bg_layer          [unchanged] solid OMNI_BG fill
 ├── frame layer         [unchanged] chrome decoration (currently disabled)
 ├── comp_scroll layer   [NEW] owns two complication slots + slide animation
 ├── comp_mask layer     [NEW] static inverse mask — fills non-diamond corners with OMNI_BG
 └── door layer          [unchanged] animated X-door, topmost
```

`s_comp_layer` in `main.c` is removed and replaced by `comp_scroll` + `comp_mask`. The door layer is completely untouched.

**Why this ordering:** `comp_scroll` draws freely across the full layer bounds. `comp_mask` paints the four non-diamond corner triangles over it in `OMNI_BG`, creating a hard diagonal clip edge. The door layer animates on top of both, unaffected.

---

## New Files

- `src/c/comp_scroll.c` / `src/c/comp_scroll.h`
- `src/c/comp_mask.c` / `src/c/comp_mask.h`

---

## Module: comp_mask

### Purpose
Draw a static inverse mask over the non-diamond corner areas so complication content cannot bleed outside the diamond opening, regardless of what the complication draws or how it slides.

### Public API
```c
void comp_mask_init(Layer *parent, GRect diamond_bounds);
void comp_mask_deinit(void);
```

No mark_dirty, no animation, no public state. The layer draws once and never changes.

### Geometry
The diamond is a rhombus whose four vertices are derived from `diamond_bounds`:

```
top_mid    = (origin.x + w/2,     origin.y)
right_mid  = (origin.x + w,       origin.y + h/2)
bottom_mid = (origin.x + w/2,     origin.y + h)
left_mid   = (origin.x,           origin.y + h/2)
```

Four corner triangles filled with `OMNI_BG`:

| Triangle    | Points                                      |
|-------------|---------------------------------------------|
| Top-left    | `(origin.x, origin.y)`, `top_mid`, `left_mid`  |
| Top-right   | `top_mid`, `(origin.x+w, origin.y)`, `right_mid` |
| Bottom-right| `right_mid`, `(origin.x+w, origin.y+h)`, `bottom_mid` |
| Bottom-left | `bottom_mid`, `(origin.x, origin.y+h)`, `left_mid` |

All four are drawn as filled `GPath` polygons (3 points each) using `gpath_draw_filled`.

The layer covers `layer_get_bounds(parent)` (full window bounds). The diamond vertex coordinates are in the parent's coordinate space and passed in at init time — no hardcoded pixels.

### Implementation notes
- Create four `GPathInfo` / `GPath` structs at init, destroy at deinit.
- The draw callback fills all four triangles. It is a normal `layer_set_update_proc` callback — it fires whenever Pebble invalidates the layer (e.g. full-window redraws during animation). The output is always identical so there is no correctness concern about when it runs.
- Layer is sized to parent bounds (full screen), same as all other layers.
- `comp_mask` has no public `mark_dirty` function — callers never need to invalidate it explicitly.

---

## Module: comp_scroll

### Purpose
Own the visible complication area. Render the outgoing and incoming complication at animated horizontal offsets so they slide left/right. Fire a callback when the transition completes so `main.c` can commit the new index to the registry.

### Public API
```c
typedef void (*CompScrollDoneCb)(int new_idx);

void comp_scroll_init(Layer *parent, GRect diamond_bounds);
void comp_scroll_deinit(void);

/* Trigger a left-slide from from_idx to to_idx. Safe to call during an
 * in-progress transition — the current outgoing frame is snapped and the
 * new transition starts from where it stopped. */
void comp_scroll_trigger(int from_idx, int to_idx, CompScrollDoneCb cb);

void comp_scroll_mark_dirty(void);
```

### Internal state
```c
static Layer   *s_layer;
static GRect    s_diamond;          // diamond bounds, stored at init
static int      s_out_idx;          // complication index sliding out
static int      s_in_idx;           // complication index sliding in
static int      s_offset;           // current x offset: 0 = settled, animating: w→0
static bool     s_transitioning;
static Animation *s_anim;
static CompScrollDoneCb s_done_cb;
```

When NOT transitioning, only `s_out_idx` is drawn at offset 0 (normal idle state).  
When transitioning, `s_out_idx` draws at `s_offset - diamond.size.w` and `s_in_idx` draws at `s_offset`. `s_offset` animates from `diamond.size.w` → `0`.

### Draw callback

`s_offset` runs from `diamond.size.w` → `0` during animation.

```
out_x = diamond.origin.x + s_offset - diamond.size.w
in_x  = diamond.origin.x + s_offset
```

Verification:

| State           | s_offset | out_x              | in_x               |
|-----------------|----------|--------------------|--------------------|
| Start of slide  | w        | origin.x           | origin.x + w       |
| End of slide    | 0        | origin.x - w       | origin.x           |

At the start the outgoing comp is at rest (origin.x) and incoming is off-screen right. At the end the outgoing is off-screen left and the incoming has settled at origin.x.

```
layer_draw(layer, ctx):
    if not s_transitioning:
        registry_draw_index(s_out_idx, ctx, s_diamond)
        return

    out_bounds = GRect(s_diamond.origin.x + s_offset - s_diamond.size.w,
                       s_diamond.origin.y, s_diamond.size.w, s_diamond.size.h)
    in_bounds  = GRect(s_diamond.origin.x + s_offset,
                       s_diamond.origin.y, s_diamond.size.w, s_diamond.size.h)

    registry_draw_index(s_out_idx, ctx, out_bounds)
    registry_draw_index(s_in_idx,  ctx, in_bounds)
```

Both complication draws use `GRect(x, s_diamond.origin.y, s_diamond.size.w, s_diamond.size.h)` as their bounds parameter. Content drawn outside the layer's actual screen area is naturally clipped by Pebble's framebuffer bounds. The `comp_mask` layer above handles diagonal masking.

### Animation
```c
static const AnimationImplementation s_anim_impl = { .update = anim_update };

anim_update(anim, progress):
    // progress runs 0 → ANIMATION_NORMALIZED_MAX; invert so s_offset runs w → 0
    s_offset = s_diamond.size.w -
               (progress * s_diamond.size.w) / ANIMATION_NORMALIZED_MAX
    layer_mark_dirty(s_layer)

anim_stopped(anim, finished, ctx):
    s_anim = NULL
    animation_destroy(anim)
    if not finished: return
    s_out_idx = s_in_idx
    s_transitioning = false
    s_offset = 0
    layer_mark_dirty(s_layer)
    if s_done_cb: s_done_cb(s_out_idx)
```

Duration: **250 ms**, curve: `AnimationCurveEaseInOut`.

If `comp_scroll_trigger` is called during an active transition, the current animation is unscheduled (triggers `anim_stopped` with `finished=false`). Because `finished=false`, the done callback is NOT fired and `s_out_idx` is NOT updated — it stays at the original outgoing index. The new transition then starts `s_out_idx → new to_idx` from the beginning. This means rapid taps skip intermediate complications rather than queuing them, which matches the existing non-animated behaviour.

### Deinit
Unschedule animation, destroy GPaths (none in this module), destroy layer. Set all pointers to NULL.

---

## Changes to registry.c

### New function
```c
void registry_draw_index(int idx, GContext *ctx, GRect bounds);
```

Draws `*s_comps[idx]` using its vtable draw function. `idx` is clamped to `[0, N_COMPS-1]` defensively.

### Updated function
```c
void registry_draw(GContext *ctx, GRect bounds) {
    registry_draw_index(s_current, ctx, bounds);
}
```

### New setter
```c
void registry_set_current(int idx) {
    s_current = ((idx % N_COMPS) + N_COMPS) % N_COMPS;
}
```

`registry_next()` is removed from `main.c`'s tap handler and replaced with `comp_scroll_trigger`. It can remain in `registry.h` for potential other callers but is no longer called on tap.

### registry.h additions
```c
void registry_draw_index(int idx, GContext *ctx, GRect bounds);
void registry_set_current(int idx);
int  registry_current_idx(void);   // returns s_current — needed by comp_scroll_trigger
```

---

## Changes to main.c

### Window load
```c
// Remove:
s_comp_layer = layer_create(bounds);
layer_set_update_proc(s_comp_layer, comp_draw);
layer_add_child(root, s_comp_layer);

// Add:
comp_scroll_init(root, s_diamond_bounds);
comp_mask_init(root, s_diamond_bounds);
```

Layer add order: `comp_scroll_init` adds its layer first, `comp_mask_init` second — this ensures mask is drawn on top of scroll content.

### Window unload
```c
// Remove:
if (s_comp_layer) { layer_destroy(s_comp_layer); s_comp_layer = NULL; }

// Add:
comp_mask_deinit();
comp_scroll_deinit();
```

### Tap handler — door open branch
```c
// Remove:
registry_next();
layer_mark_dirty(s_comp_layer);
if (s_seq == SEQ_HOLD || s_seq == SEQ_WARN) { ... reset hold timer ... }

// Replace with:
int from = registry_current_idx();
int to   = (from + 1) % N_COMPS;  // or expose registry_peek_next()
comp_scroll_trigger(from, to, on_scroll_done);
if (s_seq == SEQ_HOLD || s_seq == SEQ_WARN) { ... reset hold timer ... }
```

### New callback in main.c
```c
static void on_scroll_done(int new_idx) {
    registry_set_current(new_idx);
}
```

### comp_draw removal
The static `comp_draw` function and `s_comp_layer` are removed entirely. The `mark_all_dirty` helper is updated:

```c
static void mark_all_dirty(void) {
    door_mark_dirty();
    frame_mark_dirty();
    comp_scroll_mark_dirty();
}
```

---

## Dynamic Resolution in Complication Draw Functions

All three complication draw functions (`w_draw`, `d_draw`, `b_draw`) currently use hardcoded pixel values. They are rewritten to derive every measurement proportionally from the `bounds` parameter.

### Reference unit
```c
int u = MIN(bounds.size.w, bounds.size.h) / 10;
```

`u` is the base unit. All layout values are multiples or fractions of `u`. On basalt (diamond = 144×168), `u ≈ 14`. On chalk (180×180), `u = 18`. On emery (200×228), `u = 20`.

### comp_weather
| Value | Old | New |
|-------|-----|-----|
| Temp rect width | 80 | `u * 6` |
| Temp rect height | 36 | `u * 3` |
| Temp rect origin y | `cy - 30` | `cy - u * 2` |
| Cond rect width | 88 | `u * 7` |
| Cond rect height | 28 | `u * 2` |
| Cond rect origin y | `cy + 10` | `cy + u / 2` |

### comp_date
| Value | Old | New |
|-------|-----|-----|
| Day rect width | 120 | `bounds.size.w * 5/6` |
| Day rect height | 32 | `u * 2` |
| Day rect origin y | `cy - 40` | `cy - u * 3` |
| Date rect width | 140 | `bounds.size.w` |
| Date rect height | 36 | `u * 3` |
| Date rect origin y | `cy - 5` | `cy - u / 3` |

### comp_battery
| Value | Old | New |
|-------|-----|-----|
| Text rect width | 100 | `u * 7` |
| Text rect height | 42 | `u * 3` |
| Text rect origin y | `cy - 35` | `cy - u * 2` |
| Bar width | 80 | `u * 6` |
| Bar height | 8 | `MAX(4, u / 2)` |
| Bar origin y | `cy + 12` | `cy + u` |

Font choices remain unchanged — Pebble system fonts are fixed-size and don't scale with resolution. Font selection per complication (LECO, Gothic, Roboto) is appropriate for all target display sizes.

---

## Transition Behavior — Edge Cases

| Scenario | Behavior |
|----------|----------|
| Tap during active slide | Current animation unscheduled, new transition starts from `s_out_idx` (original) to next idx |
| Door closes during active slide | Slide animation is cancelled in `comp_scroll_deinit` (called from `window_unload`) |
| Door opens to show complication | No transition — comp appears at offset 0 as before |
| Single complication registered | Tapping still triggers a slide to the same complication (wraps to itself); `N_COMPS == 1` edge case handled gracefully |

---

## Files Changed Summary

| File | Change |
|------|--------|
| `src/c/comp_scroll.c` | **New** |
| `src/c/comp_scroll.h` | **New** |
| `src/c/comp_mask.c` | **New** |
| `src/c/comp_mask.h` | **New** |
| `src/c/registry.c` | Add `registry_draw_index`, `registry_set_current`, `registry_current_idx` |
| `src/c/registry.h` | Declare new functions |
| `src/c/main.c` | Replace `s_comp_layer` with scroll+mask, update tap handler, add `on_scroll_done` |
| `src/c/comp_weather.c` | Replace hardcoded pixels with `u`-based proportional layout |
| `src/c/comp_date.c` | Replace hardcoded pixels with `u`-based proportional layout |
| `src/c/comp_battery.c` | Replace hardcoded pixels with `u`-based proportional layout |
| `src/c/door.c` | No changes |
| `src/c/theme.c` | No changes |
| `src/c/frame.c` | No changes |

---

## Out of Scope

- Round (chalk) display special-casing — the diamond-on-round display is accepted as-is; the same rhombus geometry is used
- Font scaling — Pebble system fonts are fixed; no custom font loading
- Complication-specific enter/exit animations (all use the same horizontal slide)
- Backwards tap (slide right) — single direction only for now
