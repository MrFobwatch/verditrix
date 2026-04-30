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
