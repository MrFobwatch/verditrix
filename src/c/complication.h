#pragma once
#include <pebble.h>

/*
 * complication.h — the abstract base for everything that can appear
 * inside the diamond. Each concrete complication (date, battery, weather...)
 * defines its own struct that begins with `Complication super;` and
 * provides a vtable of function pointers.
 *
 * Pattern:
 *   typedef struct {
 *       Complication super;
 *       <concrete state>
 *   } MyComplication;
 *
 *   static void my_draw(Complication *base, GContext *ctx, GRect bounds) {
 *       MyComplication *self = (MyComplication *)base;
 *       ...
 *   }
 *
 *   static const ComplicationVTable s_vtable = {
 *       .init = my_init, .deinit = my_deinit,
 *       .tick = my_tick, .draw = my_draw,
 *   };
 *
 *   static MyComplication s_instance = {
 *       .super = { .vtable = &s_vtable, .name = "Mine" }
 *   };
 *
 *   Complication *comp_mine = &s_instance.super;  // exported
 */

typedef struct Complication Complication;

typedef struct {
    void (*init)  (Complication *self);
    void (*deinit)(Complication *self);
    void (*tick)  (Complication *self, struct tm *t);
    void (*draw)  (Complication *self, GContext *ctx, GRect bounds);
} ComplicationVTable;

struct Complication {
    const ComplicationVTable *vtable;
    const char *name;
};

/* Convenience wrappers — call these instead of dereferencing vtable yourself */
static inline void complication_init  (Complication *c)                       { c->vtable->init(c); }
static inline void complication_deinit(Complication *c)                       { c->vtable->deinit(c); }
static inline void complication_tick  (Complication *c, struct tm *t)         { c->vtable->tick(c, t); }
static inline void complication_draw  (Complication *c, GContext *ctx, GRect b) { c->vtable->draw(c, ctx, b); }
