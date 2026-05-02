#include "comp_mask.h"
#include "config.h"

static Layer    *s_layer     = NULL;
static GPoint    s_pts[4][4];
static GPathInfo s_infos[4]  = {
    { 4, s_pts[0] },
    { 4, s_pts[1] },
    { 4, s_pts[2] },
    { 4, s_pts[3] },
};
static GPath    *s_paths[4]  = { NULL, NULL, NULL, NULL };

/* Diamond edge endpoints for explicit stroke rendering */
static GPoint s_edge[4][2];  /* [edge][start/end] */

static void layer_draw(Layer *layer, GContext *ctx) {
    graphics_context_set_fill_color(ctx, OMNI_BG);
    for (int i = 0; i < 4; i++) {
        gpath_draw_filled(ctx, s_paths[i]);
    }

    /* Draw diamond edge strokes on top of the mask fill */
    // graphics_context_set_stroke_color(ctx, GColorYellow);
    // graphics_context_set_stroke_width(ctx, 6);
    // for (int i = 0; i < 4; i++) {
    //     graphics_draw_line(ctx, s_edge[i][0], s_edge[i][1]);
    // }
}

void comp_mask_init(Layer *parent, GRect diamond_bounds) {
    int hw       = diamond_bounds.size.w / 2;
    int cy       = diamond_bounds.origin.y + diamond_bounds.size.h / 2;
    int OV       = MASK_OVERSHOOT;

    /*
     * Derive real diamond corner positions from the same constants door.c uses.
     * At progress=100 (door open):
     *   wing_px  = x of the arm's top/bottom wing (on both sides, symmetric)
     *   waist_px = x of the arm's waist (the widest visible gap left/right)
     *
     * Right arm (forms left diamond edge): wing at (wing_px, top/bottom),
     *                                      waist at (waist_px, cy)
     * Left arm  (forms right diamond edge): mirror of right arm
     */
    int wing_px  = hw * X_WING_PCT  / 100;
    int waist_px = hw * X_WAIST_PCT / 100;

    int rx0 = diamond_bounds.origin.x - OV;
    int rx1 = diamond_bounds.origin.x + diamond_bounds.size.w + OV;
    int ry0 = diamond_bounds.origin.y - OV;
    int ry1 = diamond_bounds.origin.y + diamond_bounds.size.h + OV;

    /* diamond corner x positions, pushed OV into the diamond for overshoot */
    int left_tip  = diamond_bounds.origin.x + waist_px + OV;
    int right_tip = diamond_bounds.origin.x + diamond_bounds.size.w - waist_px - OV;
    /* diamond top/bottom wing x positions, pushed OV inward */
    int wing_l    = diamond_bounds.origin.x + wing_px  + OV;
    int wing_r    = diamond_bounds.origin.x + diamond_bounds.size.w - wing_px - OV;

    /* Diamond edge endpoints (inner boundary of each mask quad) */
    s_edge[0][0] = GPoint(wing_l,    ry0);  s_edge[0][1] = GPoint(left_tip,  cy);  /* TL */
    s_edge[1][0] = GPoint(wing_r,    ry0);  s_edge[1][1] = GPoint(right_tip, cy);  /* TR */
    s_edge[2][0] = GPoint(right_tip, cy);   s_edge[2][1] = GPoint(wing_r,    ry1); /* BR */
    s_edge[3][0] = GPoint(left_tip,  cy);   s_edge[3][1] = GPoint(wing_l,    ry1); /* BL */

    /* top-left: screen corner → top-left wing → left waist tip → left screen edge at cy */
    s_pts[0][0] = GPoint(rx0,      ry0);
    s_pts[0][1] = GPoint(wing_l,   ry0);
    s_pts[0][2] = GPoint(left_tip, cy);
    s_pts[0][3] = GPoint(rx0,      cy);

    /* top-right: top-right wing → screen corner → right screen edge at cy → right waist tip */
    s_pts[1][0] = GPoint(wing_r,    ry0);
    s_pts[1][1] = GPoint(rx1,       ry0);
    s_pts[1][2] = GPoint(rx1,       cy);
    s_pts[1][3] = GPoint(right_tip, cy);

    /* bottom-right: right waist tip → right screen edge at cy → screen corner → bottom-right wing */
    s_pts[2][0] = GPoint(right_tip, cy);
    s_pts[2][1] = GPoint(rx1,       cy);
    s_pts[2][2] = GPoint(rx1,       ry1);
    s_pts[2][3] = GPoint(wing_r,    ry1);

    /* bottom-left: left screen edge at cy → left waist tip → bottom-left wing → screen corner */
    s_pts[3][0] = GPoint(rx0,      cy);
    s_pts[3][1] = GPoint(left_tip, cy);
    s_pts[3][2] = GPoint(wing_l,   ry1);
    s_pts[3][3] = GPoint(rx0,      ry1);

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

void comp_mask_set_visible(bool visible) {
    if (s_layer) layer_set_hidden(s_layer, !visible);
}

void comp_mask_set_split(int dx) {
    if (!s_layer) return;
    gpath_move_to(s_paths[0], GPoint(-dx, 0));
    gpath_move_to(s_paths[3], GPoint(-dx, 0));
    gpath_move_to(s_paths[1], GPoint( dx, 0));
    gpath_move_to(s_paths[2], GPoint( dx, 0));
    layer_mark_dirty(s_layer);
}
