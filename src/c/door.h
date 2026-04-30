#pragma once
#include <pebble.h>

/*
 * door.h — the animated X-half doors.
 *
 * Two 5-point polygons (left/right X-halves). When closed they cover the
 * diamond area forming the X shape; when opening each half slides outward
 * revealing the complication underneath. Time is rendered on the halves
 * and slides away with them.
 *
 * State machine:
 *   CLOSED ─door_open()→ OPENING ─anim_done→ OPEN ─door_close()→ CLOSING ─anim_done→ CLOSED
 *
 * Caller drives state transitions. Door fires a callback on stable states.
 */

typedef enum {
    DOOR_CLOSED,
    DOOR_OPENING,
    DOOR_OPEN,
    DOOR_CLOSING
} DoorState;

typedef void (*DoorStateCb)(DoorState state, void *ctx);

/* Construct: parent layer + the diamond's bounding rect (where doors live) */
void door_init(Layer *parent, GRect diamond_bounds);
void door_deinit(void);

void door_open(void);
void door_close(void);

DoorState door_state(void);
void      door_mark_dirty(void);

/* progress is 0..100 (closed..open). Exposed so other modules
 * (e.g., diamond outline) can sync to the same animation. */
int door_progress(void);

void door_set_state_cb(DoorStateCb cb, void *ctx);

/* Called every animation frame (≈ 30Hz during open/close) so other
 * synced layers (diamond outline, complication) can mark themselves
 * dirty in lockstep with the door's own redraws. NULL to disable. */
typedef void (*DoorFrameCb)(int progress, void *ctx);
void door_set_frame_cb(DoorFrameCb cb, void *ctx);

/* Update time text rendered on the doors. hh and mm should be
 * 2-character strings (e.g. "09", "47"). NULL for either is ignored. */
void door_set_time(const char *hh, const char *mm);
