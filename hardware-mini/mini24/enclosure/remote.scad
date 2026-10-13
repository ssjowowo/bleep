// Bleep mini24 enclosure -- v1's "Pebble" shell (hardware/enclosure/remote.scad, copied and
// adapted) for the 2.4" BuyDisplay panel. All dimensions come from params.scad, generated
// by ../layout.py. Changes from v1: microphone boss + port hole in the row under the
// screen, fins on the back plate that catch the LCD, a ledge on the back plate under the
// board's top end, the battery connector + plug, a thinner back-plate lip, a reset slot,
// and checks of the folded FPC against the board's top-side parts.
// 2026-10-13: the cell moved from under the screen to under the KEYS. A printed DECK
// (its own part) lies between the cell and the board there: screwed to the board by the
// two bottom board screws, carried at its edges by two rails on the back plate beside
// the cell, and holding the back plate's two screws in inserts in its own bosses. The
// plate carries pillars under the board under the screen instead.
//
// Render a part from the command line:
//   openscad -D 'part="front"' -o stl/front_shell.stl remote.scad
// parts: front, ir_window, sensor_window, back, deck, caps, caps_labels, assembly, interference
//
// Coordinates: layout.py uses shell coords with y pointing DOWN from the
// top (IR) end. Here everything is placed at [x, -y] so the top end is +y.

include <params.scad>

part = "assembly";
labels = true;          // engrave key labels on the caps
$fn = 48;

EF = 2.0;               // front edge round-over
EB = 1.0;               // back edge round-over
EPS = 0.01;

function S(p) = [p[0], -p[1]];

// ---------------------------------------------------------------- outline
module outline2d(off = 0) {
    hull() {
        for (c = [[R_TOP, R_TOP], [W - R_TOP, R_TOP]]) translate(S(c)) circle(r = max(R_TOP + off, 0.1));
        for (c = [[R_BOT, L - R_BOT], [W - R_BOT, L - R_BOT]]) translate(S(c)) circle(r = max(R_BOT + off, 0.1));
    }
}

// Full rounded solid, z 0..H. The outline is convex, so a hull of offset
// slices gives the round-overs on the front and back edges.
module solid() {
    n = 6;
    hull() {
        for (i = [0:n]) {
            a = 90 * i / n;
            translate([0, 0, H - EF + EF * sin(a) - EPS]) linear_extrude(EPS) outline2d(-EF + EF * cos(a));
            translate([0, 0, EB - EB * sin(a)]) linear_extrude(EPS) outline2d(-EB + EB * cos(a));
        }
    }
}

module slab(z0, z1) {
    intersection() {
        solid();
        translate([-1, -L - 1, z0]) cube([W + 2, L + 2, z1 - z0]);
    }
}

module rrect(size, r, center_p) {
    translate(S(center_p)) offset(r = r) square([size[0] - 2 * r, size[1] - 2 * r], center = true);
}

// ---------------------------------------------------------------- key caps
module pill2d(w, h) { hull() for (s = [-1, 1]) translate([s * (w / 2 - h / 2), 0]) circle(d = h); }

module halfplane_ccw(theta, g) { rotate(theta) translate([-60, g]) square([120, 60]); }
module halfplane_cw(theta, g)  { rotate(theta) translate([-60, -60 - g]) square([120, 60]); }

module wedge2d(angle, g) {
    intersection() { halfplane_ccw(angle - 45, g); halfplane_cw(angle + 45, g); }
}

module annulus2d(r0, r1) { difference() { circle(r = r1, $fn = 96); circle(r = r0, $fn = 96); } }

seg_g = DPAD_BRIDGE_W / 2 + CAP_HOLE_CLR;

// 2D outline of a cap, absolute position. k = [name, type, sw_pos, angle, label]
module cap2d(k) {
    t = k[1];
    if (t == "ring") translate(S(DPAD_C)) annulus2d(DPAD_R_IN, DPAD_R_OUT);
    else if (t == "ok") translate(S(DPAD_C)) circle(r = DPAD_OK_R, $fn = 96);
    else if (t == "round") translate(S(k[2])) circle(r = ROUND_R);
    else if (t == "pill") translate(S(k[2])) pill2d(PILL[0], PILL[1]);
    else if (t == "volL" || t == "volR")
        translate(S(VOL_C)) intersection() {
            pill2d(VOL_BAR[0], VOL_BAR[1]);
            translate([t == "volL" ? -VOL_BAR[0] / 2 - 1 : VOL_SPLIT / 2, -VOL_BAR[1]])
                square([VOL_BAR[0] / 2 + 1 - VOL_SPLIT / 2, 2 * VOL_BAR[1]]);
        }
}

// Outline of the hole in the front wall for a cap (before clearance).
module hole_base2d(k) {
    t = k[1];
    if (t == "volL" || t == "volR") translate(S(VOL_C)) pill2d(VOL_BAR[0], VOL_BAR[1]);
    else if (t == "ring") translate(S(DPAD_C)) circle(r = DPAD_R_OUT, $fn = 96);     // one hole for ring + OK
    else cap2d(k);
}

// Where a cap's flange may extend (keeps flanges off bridges and neighbours).
module flange_mask2d(k) {
    t = k[1];
    if (t == "ring") translate(S(DPAD_C)) annulus2d(DPAD_R_OUT - 1, 60);       // flange only outward
    else if (t == "ok") translate(S(DPAD_C)) circle(r = 0.01);                  // OK has its own flange under the ring
    else if (t == "volL" || t == "volR")
        translate(S(VOL_C)) translate([t == "volL" ? -40 - VOL_SPLIT / 2 : VOL_SPLIT / 2, -20]) square([40, 40]);
    else translate(S(k[2])) square(60, center = true);
}

// Key icons, about 4.5 mm across (ICON_F x the 6 mm drawings below), centred
// on the origin. Line strokes stay 0.65 mm whatever the scale and every gap
// stays >= 0.5 mm, so the inlays still print with a 0.4 mm nozzle.
ICON_F = 0.75;
module icon2d(name) {
    f = ICON_F;
    s = 0.65;  // stroke
    if (name == "@back") {
        // return arrow: U-turn on the right, arrowhead pointing left at the top
        r = 2.0 * f;
        translate([0, -0.4 * f]) {
            translate([0.7 * f, 0]) intersection() {
                annulus2d(r - s / 2, r + s / 2);
                translate([0, -5]) square(10);
            }
            translate([-1.0 * f, r - s / 2]) square([1.7 * f, s]);          // top stem
            translate([-1.6 * f, -r - s / 2]) square([2.3 * f, s]);         // bottom stem
            polygon([[-2.9 * f, r], [-1.0 * f, r + 1.3 * f], [-1.0 * f, r - 1.3 * f]]);   // arrowhead
        }
    } else if (name == "@home") {
        scale(f) difference() {
            union() {
                polygon([[-3.1, 0.3], [3.1, 0.3], [0, 3.2]]);         // roof
                translate([-2.1, -2.9]) square([4.2, 3.3]);            // body
            }
            translate([-0.6, -2.95]) square([1.2, 1.9]);               // door
        }
    } else if (name == "@mute") {
        translate([-0.6 * f, 0]) {
            scale(f) {
                translate([-2.5, -1.0]) square([1.1, 2.0]);            // speaker body
                polygon([[-1.4, -1.0], [-1.4, 1.0], [0.4, 2.4], [0.4, -2.4]]);   // cone
            }
            for (a = [45, -45]) translate([2.3 * f, 0]) rotate(a) square([3.0 * f, s], center = true);   // X
        }
    } else if (name == "@youtube") {
        // play button: rounded rectangle with the triangle left in the cap colour
        scale(f) difference() {
            offset(r = 1.2) square([4.4, 2.2], center = true);
            polygon([[-0.8, -1.15], [-0.8, 1.15], [1.25, 0]]);
        }
    } else if (name == "@netflix") {
        // bold N
        scale(f) {
            translate([-1.7, -2.5]) square([0.95, 5.0]);
            translate([0.75, -2.5]) square([0.95, 5.0]);
            polygon([[-1.7, 2.5], [-0.75, 2.5], [1.7, -2.5], [0.75, -2.5]]);
        }
    } else if (name == "@plex") {
        // chevron
        scale(f) polygon([[-1.5, 2.6], [0.1, 2.6], [2.0, 0], [0.1, -2.6], [-1.5, -2.6], [0.4, 0]]);
    }
}

// Labels and icons are two-colour inlays: the cap body has a recess and a
// separate label body fills it flush with the key top (print both in one job
// on a multi-material printer, labels in the second colour).
LABEL_DEPTH = 0.6;   // 3 layers at 0.2 mm, so a light colour hides a dark body

// 2D outline of a cap's label/icon, absolute position.
module cap_label2d(k) {
    t = k[1];
    if (t == "ring") {
        // four dots mark the press points (drawn once, with the UP entry)
        if (k[0] == "UP") translate(S(DPAD_C)) for (a = [0:90:270]) rotate(a)
            translate([(DPAD_R_IN + DPAD_R_OUT) / 2, 0]) circle(r = 0.75, $fn = 24);
    } else if (labels && len(k[4]) > 0 && k[4][0] == "@") {
        translate(S(k[2])) icon2d(k[4]);
    } else if (labels && (t == "volL" || t == "volR")) {
        // + / - drawn as bars (font glyphs sit off the cap's centre line)
        translate(S([VOL_C[0] + (t == "volL" ? -1 : 1) * VOL_BAR[0] / 4, VOL_C[1]])) {
            square([3.2, 0.75], center = true);
            if (t == "volR") square([0.75, 3.2], center = true);
        }
    } else if (labels && k[4] != "") {
        p = (t == "ok") ? DPAD_C : k[2];
        sz = t == "ok" ? 2.6 : 2.0;
        translate(S(p)) text(k[4], size = sz, font = "Liberation Sans:style=Bold", halign = "center", valign = "center");
    }
}

module cap_engrave(k) {   // recess cut into the cap body
    translate([0, 0, H + CAP_PROTRUDE - LABEL_DEPTH]) linear_extrude(LABEL_DEPTH + 1) cap_label2d(k);
}

module cap_label(k) {     // the inlay that fills the recess, flush with the top
    translate([0, 0, H + CAP_PROTRUDE - LABEL_DEPTH]) linear_extrude(LABEL_DEPTH)
        intersection() { cap_label2d(k); cap2d(k); }
}

// Generic cap: body through the wall, 45-degree seat that sits in a matching
// countersink on the inside of the wall, flange plate, stem onto the switch.
// Printed top-down, so the seat is the only overhang and it is 45 degrees.
module cap_generic(shape_children_k, stems) {
    k = shape_children_k;
    nz = 4;
    difference() {
        union() {
            // body
            translate([0, 0, Z_FRONT_IN + CAP_FLANGE]) linear_extrude(H + CAP_PROTRUDE - Z_FRONT_IN - CAP_FLANGE)
                offset(delta = -0.0) cap2d(k);
            // 45-degree seat inside the wall countersink
            for (i = [0:nz - 1]) {
                f = CAP_FLANGE * (1 - i / nz);
                translate([0, 0, Z_FRONT_IN + CAP_FLANGE * i / nz - EPS]) linear_extrude(CAP_FLANGE / nz + 2 * EPS)
                    union() { cap2d(k); intersection() { offset(r = f) cap2d(k); flange_mask2d(k); } }
            }
            // flange plate under the wall
            translate([0, 0, Z_FRONT_IN - CAP_FLANGE]) linear_extrude(CAP_FLANGE + EPS)
                union() { cap2d(k); intersection() { offset(r = CAP_FLANGE) cap2d(k); flange_mask2d(k); } }
            // stem(s) onto the switch actuator
            for (s = stems) translate([s[0], -s[1], Z_ACTUATOR + STEM_GAP])
                cylinder(d = 3.4, h = Z_FRONT_IN - CAP_FLANGE - Z_ACTUATOR - STEM_GAP + EPS);
        }
        cap_engrave(k);
        // soften the top edge a little
    }
}

// The OK button: no wall to seat on (it shares the ring's hole), so its flange
// sits DPAD_OK_DROP under the ring's inner edge, overlapping it by DPAD_OK_LIP.
// (mini24: a 45-degree cone from the body down to the flange, so the cap prints
// top-down without a flat 1.6 mm overhang; it stays 0.2 mm inside the ring.)
module ok_cap(k) {
    fl_top = Z_FRONT_IN - CAP_FLANGE - DPAD_OK_DROP;
    rf = DPAD_R_IN + DPAD_OK_LIP;
    difference() {
        union() {
            translate([0, 0, fl_top]) linear_extrude(H + CAP_PROTRUDE - fl_top) cap2d(k);
            translate([DPAD_C[0], -DPAD_C[1], fl_top - CAP_FLANGE]) cylinder(r = rf, h = CAP_FLANGE + EPS, $fn = 96);
            translate([DPAD_C[0], -DPAD_C[1], fl_top - EPS]) cylinder(r1 = rf, r2 = DPAD_OK_R, h = rf - DPAD_OK_R, $fn = 96);
            translate([DPAD_C[0], -DPAD_C[1], Z_ACTUATOR + STEM_GAP]) cylinder(d = 3.4, h = fl_top - CAP_FLANGE - Z_ACTUATOR - STEM_GAP + EPS);
        }
        cap_engrave(k);
    }
}

// The ring is one cap with a stem on each direction switch (built with the
// UP entry; the other three ring entries add nothing).
module cap(k) {
    if (k[1] == "ring") { if (k[0] == "UP") cap_generic(k, [for (kk = KEYS) if (kk[1] == "ring") kk[2]]); }
    else if (k[1] == "ok") ok_cap(k);
    else cap_generic(k, [k[2]]);
}

// PWR key: pill cap in the row under the screen, same seat/flange system.
PWR_K = ["PWR", "pwr", PWR_KEY, 0, ""];
module pwr2d() { translate(S(PWR_KEY)) pill2d(PWR_PILL[0], PWR_PILL[1]); }

module pwr_cap() {
    nz = 4;
    union() {
        translate([0, 0, Z_FRONT_IN + CAP_FLANGE]) linear_extrude(H + CAP_PROTRUDE - Z_FRONT_IN - CAP_FLANGE) pwr2d();
        for (i = [0:nz - 1]) translate([0, 0, Z_FRONT_IN + CAP_FLANGE * i / nz - EPS])
            linear_extrude(CAP_FLANGE / nz + 2 * EPS) offset(r = CAP_FLANGE * (1 - i / nz)) pwr2d();
        translate([0, 0, Z_FRONT_IN - CAP_FLANGE]) linear_extrude(CAP_FLANGE + EPS) offset(r = CAP_FLANGE) pwr2d();
        translate([PWR_KEY[0], -PWR_KEY[1], Z_ACTUATOR + STEM_GAP]) cylinder(d = 3.4, h = Z_FRONT_IN - CAP_FLANGE - Z_ACTUATOR - STEM_GAP + EPS);
    }
}

// ---------------------------------------------------------------- front shell
// Cuts applied after the internal posts/bosses are added (the cavity itself
// is removed earlier, in front_shell, so it doesn't eat the posts).
module front_cuts() {
    // glass pocket, sized on the lens (the panel stack sits in the cavity below it)
    translate([0, 0, Z_FRONT_IN - 1]) linear_extrude(Z_LENS_TOP - Z_FRONT_IN + 1)
        rrect([GLASS[0] + 2 * CLEAR, GLASS[1] + 2 * CLEAR], LENS[2] + CLEAR, GLASS_C);
    // screen window: the frame overlaps the glass by LIP_OVER, chamfered outside
    win = [LENS[0] - 2 * LIP_OVER, LENS[1] - 2 * LIP_OVER];
    hull() {
        translate([0, 0, Z_LENS_TOP - 1]) linear_extrude(EPS) rrect(win, 0.8, LENS_C);
        translate([0, 0, H - 0.5]) linear_extrude(EPS) rrect(win, 0.8, LENS_C);
        translate([0, 0, H + EPS]) linear_extrude(EPS) rrect([win[0] + 1.0, win[1] + 1.0], 1.3, LENS_C);
    }
    // key holes + countersinks
    for (k = KEYS) key_hole(k);
    // PWR key hole
    translate([0, 0, Z_FRONT_IN - 1]) linear_extrude(H) offset(delta = CAP_HOLE_CLR) pwr2d();
    for (i = [0:3]) translate([0, 0, Z_FRONT_IN - EPS + CAP_FLANGE * i / 4])
        linear_extrude(CAP_FLANGE / 4 + 2 * EPS) offset(r = CAP_HOLE_CLR + CAP_FLANGE * (1 - i / 4)) pwr2d();
    // IR window through the top end wall (centred on the LED axis), with a
    // recess inside for the clear window's flange
    translate([IR_WINDOW[0] - IR_WINDOW[1] / 2, -WALL - 1, IR_WINDOW[3] - IR_WINDOW[2] / 2]) cube([IR_WINDOW[1], WALL + 2, IR_WINDOW[2]]);
    translate([IR_WINDOW[0] - IR_WINDOW[1] / 2 - IR_FLANGE[0], -WALL - 0.01, IR_WINDOW[3] - IR_WINDOW[2] / 2 - IR_FLANGE[1]])
        cube([IR_WINDOW[1] + 2 * IR_FLANGE[0], IR_FLANGE[2] + 0.01, IR_WINDOW[2] + 2 * IR_FLANGE[1]]);
    // USB-C: plug opening through the bottom wall, and a pocket inside that
    // thins the wall to USB_WALL_T so the receptacle face sits close to the outside
    translate([USB_X, -L - 1, USB_Z]) rotate([-90, 0, 0]) linear_extrude(WALL + 2)
        offset(r = USB_OPEN[1] / 2 - 0.01) square([USB_OPEN[0] - USB_OPEN[1], 0.02], center = true);
    // (kept 0.8 inside the outer surface where the bottom corners curve in)
    intersection() {
        translate([USB_X - 7.0, -(L - USB_WALL_T), BACK_T - 1]) cube([14.0, WALL - USB_WALL_T + 1, min(USB_Z + 4.0, Z_FRONT_IN) - BACK_T + 1]);
        translate([0, 0, -1]) linear_extrude(H + 2) outline2d(-0.8);
    }
    // only the receptacle's own width (+-4.6; it is +-4.47) is thinned to USB_WALL_T
    translate([USB_X - 4.6, -(L - USB_WALL_T), BACK_T - 1]) cube([9.2, WALL - USB_WALL_T + 1, min(USB_Z + 4.0, Z_FRONT_IN) - BACK_T + 1]);
    // sensor window under the screen: a pill recess for the clear window, and a
    // hole through it over the IR receiver (learning) and the light sensor
    translate([0, 0, H - WIN_DEPTH]) linear_extrude(1) sensor_win2d();
    for (h = SENSOR_HOLES) translate([h[0], -h[1], Z_FRONT_IN - 1]) cylinder(d = h[2], h = H, $fn = 40);
    // microphone port: a 1.0 mm hole through the wall and the boss, over the mic's port
    translate([MIC_PORT[0], -MIC_PORT[1], mic_boss_z0() - 1]) cylinder(d = MIC_HOLE_D, h = H, $fn = 24);
    // a small countersink outside so the hole doesn't clog with dust
    translate([MIC_PORT[0], -MIC_PORT[1], H - 0.4]) cylinder(d1 = MIC_HOLE_D, d2 = MIC_HOLE_D + 0.8, h = 0.4 + EPS, $fn = 24);
    // grooves in the top wall for the back plate's snap fingers
    for (t = TOP_TABS) translate([t[0] - t[1] / 2 - 0.3, -(WALL + SNAP_GAP), SNAP_Z])
        rotate([0, 90, 0]) linear_extrude(t[1] + 0.6) polygon([[0.75, 0], [-0.75, 0], [0, SNAP_BUMP + 0.1]]);
    // heat-set insert holes (from the backplane side)
    for (p = concat(KEY_HOLES, TOP_BOSSES, BOT_BOSSES)) translate([p[0], -p[1], Z_BP_TOP - EPS]) cylinder(d = INSERT_HOLE_D, h = INSERT_HOLE_DEPTH);
}

module key_hole(k) {
    translate([0, 0, Z_FRONT_IN - 1]) linear_extrude(H) offset(delta = CAP_HOLE_CLR) hole_base2d(k);
    for (i = [0:3]) translate([0, 0, Z_FRONT_IN - EPS + CAP_FLANGE * i / 4])
        linear_extrude(CAP_FLANGE / 4 + 2 * EPS) intersection() {
            offset(r = CAP_HOLE_CLR + CAP_FLANGE * (1 - i / 4)) hole_base2d(k);
            offset(delta = CAP_HOLE_CLR) flange_mask2d(k);
        }
}

// the lens: as wide as the BL, 0.2 shorter, flush with it at the FPC end
GLASS_C = [LENS_C[0], LENS_C[1] + (LENS[1] - GLASS[1]) / 2];

// the mic boss ends this high: the mic's lid + the compressed foam gasket ring
function mic_boss_z0() = Z_BP_TOP + MIC_SIZE[2] + MIC_GASKET[2];

module front_adds() {
    // posts that land on the board and the bottom bosses; all take inserts
    for (p = KEY_HOLES) translate([p[0], -p[1], Z_BP_TOP]) cylinder(d = POST_D, h = Z_FRONT_IN - Z_BP_TOP + EPS);
    for (p = concat(TOP_BOSSES, BOT_BOSSES)) translate([p[0], -p[1], Z_BP_TOP]) cylinder(d = BOSS_D, h = Z_FRONT_IN - Z_BP_TOP + EPS);
    // mic boss: carries the port hole down to the gasket on the mic's lid
    translate([MIC_PORT[0], -MIC_PORT[1], mic_boss_z0() + 0.5]) cylinder(d = MIC_BOSS_D, h = Z_FRONT_IN - mic_boss_z0() - 0.5 + EPS, $fn = 32);
    translate([MIC_PORT[0], -MIC_PORT[1], mic_boss_z0()]) cylinder(d = MIC_GASKET[0], h = 0.5 + EPS, $fn = 32);   // neck = the gasket's OD
    // (nothing on the side walls: the panel goes in from the back past them; the
    // LCD's side edges are caught by fins on the back plate, which goes on last)
}

module front_shell() {
    difference() {
        union() {
            difference() { slab(BACK_T, H); front_cuts_cavity_only(); }
            intersection() { front_adds(); translate([0, 0, BACK_T]) linear_extrude(H) outline2d(-0.5); }
        }
        front_cuts();
    }
}

// Windows: two small parts printed separately in translucent PETG and fitted by hand
// (a drop of clear glue holds each one).
//  - sensor window: goes in from the front. The pill sits flush in its recess
//    in the front face, with a peg down through each hole; the light sensor's
//    peg runs on to just above the sensor as a light pipe (the IR receiver's
//    lens sits right under the wall anyway). Printed pill face down.
//  - IR window: goes in from the inside, before the board. A plug fills the
//    slot in the top wall, flush outside, and its flange sits in the recess on
//    the inside, so it can't be pushed out. Printed flange down.
WIN_DEPTH = 0.6;
WIN_CLR = 0.1;          // per side, between a window and the shell (press-in fit)
module sensor_win2d() { translate(S(SENSOR_WIN)) pill2d(SENSOR_WIN[2], SENSOR_WIN[3]); }
module sensor_window() {
    translate([0, 0, H - WIN_DEPTH]) linear_extrude(WIN_DEPTH) offset(delta = -WIN_CLR) sensor_win2d();
    for (i = [0:len(SENSOR_HOLES) - 1]) {
        h = SENSOR_HOLES[i];
        z0 = i == 1 ? ALS_PIPE_Z0 : Z_FRONT_IN;          // [1] is the light sensor's hole
        translate([h[0], -h[1], z0]) cylinder(d = h[2] - 2 * WIN_CLR, h = H - WIN_DEPTH - z0 + EPS, $fn = 40);
    }
}
module ir_window() {
    x0 = IR_WINDOW[0] - IR_WINDOW[1] / 2; z0 = IR_WINDOW[3] - IR_WINDOW[2] / 2;
    translate([x0 + WIN_CLR, -WALL, z0 + WIN_CLR]) cube([IR_WINDOW[1] - 2 * WIN_CLR, WALL, IR_WINDOW[2] - 2 * WIN_CLR]);
    translate([x0 - IR_FLANGE[0] + WIN_CLR, -WALL, z0 - IR_FLANGE[1] + WIN_CLR])
        cube([IR_WINDOW[1] + 2 * IR_FLANGE[0] - 2 * WIN_CLR, IR_FLANGE[2], IR_WINDOW[2] + 2 * IR_FLANGE[1] - 2 * WIN_CLR]);
}
IR_FLANGE = [1.5, 1.2, 0.8];   // the recess round the IR slot, inside the top wall: x and z overhang, depth
module windows() { sensor_window(); ir_window(); }

module front_cuts_cavity_only() { translate([0, 0, -1]) linear_extrude(Z_FRONT_IN + 1) outline2d(-WALL); }

// ---------------------------------------------------------------- back plate
module counterbored_hole(p) {
    translate([p[0], -p[1], -1]) cylinder(d = SCREW_CLR_D, h = 30, $fn = 24);
    translate([p[0], -p[1], -1]) cylinder(d = SCREW_HEAD_D, h = 1 + PLATE_HEAD_RECESS, $fn = 32);   // the whole pan head, into the standoff
}

module fence_corners(c, size, h, t = 1.0, arm = 6) {
    for (sx = [-1, 1], sy = [-1, 1]) translate([c[0] + sx * size[0] / 2, -c[1] + sy * size[1] / 2, BACK_T - EPS]) {
        translate([sx > 0 ? 0 : -t, sy > 0 ? -arm + t : -t, 0]) cube([t, arm, h]);
        translate([sx > 0 ? -arm + t : -t, sy > 0 ? 0 : -t, 0]) cube([arm, t, h]);
    }
}

// Snap fingers: there is no room for screw bosses at the top end (the glass
// runs up to it), so two fingers on the back plate's top edge click into
// grooves in the top wall. Their bump is a ridge sloped on both faces, so the
// plate snaps in and, once the screws are out, pulls off again.
SNAP_T = 1.2;                       // finger thickness (y)
SNAP_GAP = 0.15;                    // finger to wall
SNAP_TOP = BACK_T + 5.4;            // finger tip height
SNAP_BUMP = 0.4;                    // how far the ridge stands out (0.25 into the wall)
SNAP_Z = SNAP_TOP - 0.9;            // ridge height
SNAP_FILLET = 0.3;                  // root fillet on the inner face
// bending strain while it clicks in: 1.5 t d / l^2, l from the root (the lip is
// cut away round the finger) to the ridge
SNAP_STRAIN = 1.5 * SNAP_T * (SNAP_BUMP - SNAP_GAP) / pow(SNAP_Z - BACK_T, 2);
assert(SNAP_STRAIN < 0.03, str("snap strain ", SNAP_STRAIN));
assert(TOP_LEDGE[1] - (WALL + SNAP_GAP + SNAP_T + SNAP_FILLET) >= 0.45, "snap fingers (with their fillet) too close to the top ledge");
assert(SNAP_BUMP - SNAP_GAP >= 0.25, "snap catch under 0.25 mm");
module snap_fingers() {
    for (t = TOP_TABS) translate([t[0] - t[1] / 2, -(WALL + SNAP_GAP) - SNAP_T, BACK_T - EPS]) {
        cube([t[1], SNAP_T, SNAP_TOP - BACK_T]);
        // root fillet on the inner (+y, towards the ledge) face
        translate([0, -SNAP_FILLET, 0]) difference() {
            cube([t[1], SNAP_FILLET + EPS, SNAP_FILLET]);
            translate([-1, 0, SNAP_FILLET]) rotate([0, 90, 0]) cylinder(r = SNAP_FILLET, h = t[1] + 2, $fn = 24);
        }
        // the ridge on the outer (wall) face, a triangle in y-z
        translate([0, SNAP_T, SNAP_Z - BACK_T]) rotate([0, 90, 0]) linear_extrude(t[1])
            polygon([[0.6, 0], [-0.6, 0], [0, SNAP_BUMP]]);
    }
}

// The back plate (flat back): a flat 1.5 mm plate with a thin locating lip and
// the supports on it -- a ledge under the board's top end, pillars under the
// board under the screen, the LCD fins, two rails beside the cell that carry the
// deck's edges, small stops at the cell's ends and a short standoff round each
// of its two screws (they go into the deck's bosses). The board and the deck are
// screwed to the front shell on their own; snap fingers hold the plate's top end.
// Everything else between the plate and the board is open: the flat cell under
// the deck, the ESP32 module, the motor, the battery plug corner and the
// USB-C solder joints.
module back_plate() {
    lip_h = 1.5;
    sup_h = Z_BP_BOT - BACK_T + EPS;
    difference() {
        union() {
            slab(0, BACK_T);
            difference() {
                translate([0, 0, BACK_T - EPS]) linear_extrude(lip_h) difference() {
                    outline2d(-WALL - 0.25);
                    outline2d(-BP_LIP_IN);
                }
                for (p = concat(TOP_BOSSES, BOT_BOSSES)) translate([p[0], -p[1], 0]) cylinder(d = BOSS_D + 1.5, h = 10);
                translate([USB_X - 7.5, -L - 1, 0]) cube([15, WALL + 12, 10]);
                // the lip stops 0.5 short of each snap finger, so the finger flexes from the plate
                for (t = TOP_TABS) translate([t[0] - t[1] / 2 - 0.5, -(WALL + 3), BACK_T - 1]) cube([t[1] + 1.0, 3, 5]);
                // and it is cut away beside the cell's bottom corners, so a cell pushed
                // into a corner meets only a rail and a stop
                for (c = LIP_CUTS) translate([c[0], -c[3], BACK_T - 1]) cube([c[2] - c[0], c[3] - c[1], 5]);
            }
            // fins up through the board-to-wall gap that catch the LCD's side edges
            // 0.25 behind it, so a finger on the screen loads the shell, not the board
            for (f = PANEL_FINS) translate([f[0], -f[3], BACK_T - EPS]) cube([f[2] - f[0], f[3] - f[1], FIN_Z1 - BACK_T + EPS]);
            translate([0, 0, BACK_T - EPS]) linear_extrude(sup_h) intersection() {
                outline2d(-WALL - 0.25);
                union() {
                    for (r = RAILS) translate([r[0], -r[3]]) square([r[2] - r[0], r[3] - r[1]]);
                    // ledge under the board's top end
                    translate([TOP_LEDGE[0], -TOP_LEDGE[3]]) square([TOP_LEDGE[2] - TOP_LEDGE[0], TOP_LEDGE[3] - TOP_LEDGE[1]]);
                    // pillars under the board under the screen
                    for (p = PILLARS) translate(S(p)) circle(d = p[2], $fn = 32);
                }
            }
            // rails beside the cell up to RAIL_GAP under the deck: they guide the cell's
            // long sides (0.61 clear) and carry the deck's edges; clipped to the wall's curve
            translate([0, 0, BACK_T - EPS]) linear_extrude(Z_DECK_BOT - RAIL_GAP - BACK_T + EPS) intersection() {
                outline2d(-WALL - 0.25);
                union() { for (r = CELL_RAILS) translate([r[0], -r[3]]) square([r[2] - r[0], r[3] - r[1]]); }
            }
            // short standoffs round the plate's screws, under the deck's bosses
            for (p = STANDOFFS) translate([p[0], -p[1], BACK_T - EPS]) cylinder(d = p[2], h = PLATE_BOSS_H + EPS);
            // small stops at the cell's ends, 2 mm wide, the top pair well in from the
            // corners where the leads leave, 1.5 mm tall with a 0.8 mm chamfer on the
            // cell side so the plate slides over a slightly long cell
            for (b = BATT_STOPS) translate([b[0] - 1.0, b[2] > 0 ? -b[1] - 1.2 : -b[1] + 1.2, BACK_T - EPS])
                mirror([0, b[2] > 0 ? 0 : 1, 0]) hull() {
                    translate([0, 0, 0]) cube([2.0, 1.2, 0.7]);
                    translate([0, 0, 0]) cube([2.0, 0.4, 1.5]);
                }
            snap_fingers();
        }
        for (p = PLATE_SCREWS) counterbored_hole(p);
        // slot to the EN/GND reset pads on the board's underside (in the plug corner)
        translate([EN_SLOT[0], -EN_SLOT[1], -1]) linear_extrude(BACK_T + 2)
            hull() for (s = [-1, 1]) translate([s * (EN_SLOT[2] - EN_SLOT[3]) / 2, 0]) circle(d = EN_SLOT[3], $fn = 24);
    }
}

// ---------------------------------------------------------------- the deck
// A thin plate between the cell and the board under the keys: the board lies on
// it, the plate's rails carry its edges, the cell keeps its swell gap under it.
// The two bottom board screws go through it (their heads and washers clamp it to
// the board), and its two bosses take heat-set inserts from below for the back
// plate's screws. A notch under the antenna keep-out's tail, a cut-out over the
// USB-C's underside joints.
module deck2d() {
    difference() {
        intersection() {
            outline2d(-WALL - DECK_WALL_CLR);
            translate([-1, -L - 1]) square([W + 2, L + 1 - DECK_Y0]);
        }
        for (r = DECK_RELIEFS) translate([r[0], -r[3]]) square([r[2] - r[0], r[3] - r[1]]);
        for (p = DECK_SCREWS) translate(S(p)) circle(d = SCREW_CLR_D, $fn = 24);
    }
}
module deck() {
    translate([0, 0, Z_DECK_BOT]) linear_extrude(DECK_T) deck2d();
    for (p = DECK_BOSSES) translate([p[0], -p[1], Z_DECK_BOSS0]) difference() {
        cylinder(d = BOSS_D, h = Z_DECK_BOT - Z_DECK_BOSS0 + EPS);
        translate([0, 0, -EPS]) cylinder(d = INSERT_HOLE_D, h = DECK_INSERT_DEPTH + EPS, $fn = 24);
    }
}

// ---------------------------------------------------------------- parts on the print bed
module front_print() { translate([0, 0, H]) rotate([180, 0, 0]) front_shell(); }   // front face on the bed
module deck_print() { translate([0, 0, Z_BP_BOT]) rotate([180, 0, 0]) deck(); }      // board-side face on the bed, bosses up

module cap_on_bed(k, ox, oy, c) {
    // flip so the cap top sits on the bed
    translate([ox, oy, H + CAP_PROTRUDE]) rotate([180, 0, 0]) translate([-c[0], c[1], 0]) children();
}

// mode "body": the caps; mode "labels": just their inlays, in the same spots.
// Load both STLs as ONE object with two parts in the slicer (Bambu/Orca/Prusa:
// add the labels file as a part) and give the labels the second filament.
module caps_print(mode = "body") {
    for (i = [0:len(KEYS) - 1]) {
        k = KEYS[i];
        c = (k[1] == "ring" || k[1] == "ok") ? DPAD_C :
            (k[1] == "volL") ? [VOL_C[0] - VOL_BAR[0] / 4, VOL_C[1]] :
            (k[1] == "volR") ? [VOL_C[0] + VOL_BAR[0] / 4, VOL_C[1]] : k[2];
        ox = (i % 5) * 36;
        oy = -floor(i / 5) * 36;
        if (mode == "body") cap_on_bed(k, ox, oy, c) cap(k);
        else cap_on_bed(k, ox, oy, c) cap_label(k);
    }
    if (mode == "body") cap_on_bed(PWR_K, 5 * 36, 0, PWR_KEY) pwr_cap();
}

// ---------------------------------------------------------------- stand-ins for checking
module panel() {       // LCD (backlight outline) + touch glass
    translate([LCD[0], -LCD[1] - LCD[3], Z_LCD_BOT]) cube([LCD[2], LCD[3], LCD_T]);
    translate([0, 0, Z_LENS_TOP - CTP_T]) linear_extrude(CTP_T) rrect([GLASS[0], GLASS[1]], LENS[2], GLASS_C);
}
// the FPC at s mm from the BL edge, measured back from its end in J1
function fy(s_) = FPC_END_J1 + FPC_LEN - s_;
module tilted(x0, w, s0, z0, s1, z1, t) {   // a thin plate from (s0, z0) to (s1, z1), t thick (downwards)
    hull() {
        translate([x0, -fy(s0), z0 - t]) cube([w, 0.01, t]);
        translate([x0, -fy(s1), z1 - t]) cube([w, 0.01, t]);
    }
}
module panel_fpc() {   // the 50-pin FPC folded under the LCD into J1, with the touch section on it
    x0 = FPC[0]; w = FPC[1];
    zj = Z_BP_TOP + ZIF_FPC_Z;
    zt = function(s_) TOUCH_Z[0] + (FPC_TOUCH[1] - s_) * (TOUCH_Z[1] - TOUCH_Z[0]) / (FPC_TOUCH[1] - FPC_TOUCH[0]);
    // the tail's stiffener (s 22-27) flat in J1, 0.3 thick
    translate([x0, -fy(22.0), zj]) cube([w, fy(22.0) - FPC_END_J1, 0.3]);
    // 0.5 mm of flex, then the touch section (s 21.5 .. 12.5) tilted from J1's height
    // up to the LCD's back: the FPC (0.12) ...
    tilted(x0, w, 22.0, zj + 0.12, FPC_TOUCH[1], TOUCH_Z[0] + 0.12, 0.12);
    tilted(x0, w, FPC_TOUCH[1], TOUCH_Z[0] + 0.12, FPC_TOUCH[0], TOUCH_Z[1] + 0.12, 0.12);
    // ... and below it the CTP's tail, placed by the ER-TPC024-1 drawing's
    // dimensions (layout.py): the thin solder strip, and the controller box
    // with its thin J1 end (steel + CTP tail) and its 1.7 mm part (chip, caps)
    tilted(CTP_STRIP_X[0], CTP_STRIP_X[1] - CTP_STRIP_X[0], FPC_TOUCH[1], zt(FPC_TOUCH[1]), FPC_TOUCH[0], zt(FPC_TOUCH[0]), TOUCH_THIN);
    tx = [TOUCH_XY[0], TOUCH_XY[2] - TOUCH_XY[0]];
    tilted(tx[0], tx[1], FPC_TOUCH[1], zt(FPC_TOUCH[1]), TOUCH_TALL_END, zt(TOUCH_TALL_END), TOUCH_THIN);
    tilted(tx[0], tx[1], TOUCH_TALL_END, zt(TOUCH_TALL_END), FPC_TOUCH[0], zt(FPC_TOUCH[0]), FPC_PARTS_H - 0.12);
    // from the section's fold end, flat against the LCD's back to the fold
    translate([x0, -LENS_Y1, Z_LCD_BOT - 0.12]) cube([w, LENS_Y1 - fy(FPC_TOUCH[0]), 0.12]);
    // the wide section's parts area (conservative: may carry parts too), hanging down FPC_PARTS_H
    translate([FPC[6], -LENS_Y1, Z_LCD_BOT - 0.12 - FPC_PARTS_H]) cube([FPC[7], LENS_Y1 - FPC[3], 0.12 + FPC_PARTS_H - 0.05]);
    // the fold, as wide as the wide section and bulged by half the slack
    bend = FPC[4] + FPC[5] + 0.15 + FPC[8] / 2;
    translate([FPC[6], -(LENS_Y1 + bend), Z_LCD_BOT - 0.12 - FPC_PARTS_H]) cube([FPC[7], bend, Z_LCD_BOT + LCD_T - 0.1 - (Z_LCD_BOT - 0.12 - FPC_PARTS_H)]);
}
// foam pads between the board and the LCD, compressed to the gap
module foam() { for (f = FOAM_PADS) translate([f[0], -f[3], Z_BP_TOP]) cube([f[2] - f[0], f[3] - f[1], Z_LCD_BOT - Z_BP_TOP]); }
module battery() {     // the cell + its plug (PicoBlade 51021 housing) in the header's mouth
    translate([BATT_C[0] - BATT[0] / 2, -BATT_C[1] - BATT[1] / 2, BACK_T]) cube(BATT);
    translate([BATT_PLUG[0], -BATT_PLUG[3], Z_BP_BOT - 3.0]) cube([BATT_PLUG[2] - BATT_PLUG[0], BATT_PLUG[3] - BATT_PLUG[1], 2.8]);
}
module bp2d() {
    difference() {
        union() {
            outline2d(-BP_INSET);
            translate([USB_TAB[0], -USB_TAB[2]]) square([USB_TAB[1] - USB_TAB[0], 6]);
        }
    }
}
module bp_pcb() {
    translate([0, 0, Z_BP_TOP - BP_T]) linear_extrude(BP_T) difference() {
        bp2d();
        for (p = concat(KEY_HOLES, TOP_BOSSES, BOT_BOSSES)) translate(S(p)) circle(d = HOLE_D);
        // the window under the panel FPC's touch section
        translate([BOARD_WINDOW[0], -BOARD_WINDOW[3]]) square([BOARD_WINDOW[2] - BOARD_WINDOW[0], BOARD_WINDOW[3] - BOARD_WINDOW[1]]);
    }
}
module bp_switches() {
    for (k = KEYS) translate([k[2][0] - SW_BODY / 2, -k[2][1] - SW_BODY / 2, Z_BP_TOP]) cube([SW_BODY, SW_BODY, SW_HEIGHT]);
    translate([PWR_KEY[0] - SW_BODY / 2, -PWR_KEY[1] - SW_BODY / 2, Z_BP_TOP]) cube([SW_BODY, SW_BODY, SW_HEIGHT]);
}
module bp_irleds() {
    for (x = IR_LED_X) translate([x - 1.15, -(IR_LED_TIP_Y + IR_LED_LEN + 0.6), Z_BP_TOP]) cube([2.3, IR_LED_LEN + 0.6 - 0.85, 2.55]);
}
module bp_usbc() { translate([USB_X - 4.47, -USB_FACE_Y, Z_BP_TOP]) cube([8.94, 7.35, USB_H]); }
module bp_module() {   // ESP32-S3-WROOM-1 on the underside
    translate([MOD[0] - MOD[2] / 2, -MOD[1] - MOD[3] / 2, Z_BP_BOT - MOD[4]]) cube([MOD[2], MOD[3], MOD[4]]);
}
module bp_lra() { translate([MOTOR_C[0], -MOTOR_C[1], Z_BP_BOT - MOTOR_T]) cylinder(d = MOTOR_D, h = MOTOR_T); }
module bp_zif() {      // 50-pin ZIF connector under the panel
    translate([ZIF[0], -ZIF[1] - ZIF[3], Z_BP_TOP]) cube([ZIF[2], ZIF[3], ZIF[4]]);
}
module bp_ics() {
    for (p = PARTS) translate([p[0] - p[2] / 2, -p[1] - p[3] / 2, Z_BP_TOP]) cube([p[2], p[3], p[4]]);
    // the mic's foam gasket ring between its lid and the shell's boss
    translate([MIC_PORT[0], -MIC_PORT[1], Z_BP_TOP + MIC_SIZE[2]]) difference() {
        cylinder(d = MIC_GASKET[0], h = MIC_GASKET[2], $fn = 32);
        translate([0, 0, -1]) cylinder(d = MIC_GASKET[1], h = MIC_GASKET[2] + 2, $fn = 24);
    }
    // battery connector J2 on the underside (PicoBlade 53261-0271, 3.4 mm)
    translate([BATT_CONN_BOX[0], -BATT_CONN_BOX[3], Z_BP_BOT - BATT_CONN_H]) cube([BATT_CONN_BOX[2] - BATT_CONN_BOX[0], BATT_CONN_BOX[3] - BATT_CONN_BOX[1], BATT_CONN_H]);
    // its reverse-polarity FET + gate resistor beside it on the underside
    for (p = UNDER_PARTS) translate([p[0] - p[2] / 2, -p[1] - p[3] / 2, Z_BP_BOT - p[4]]) cube([p[2], p[3], p[4]]);
}
module bp_screw_heads() {   // the board screws' pan heads + washers under the board (under the deck where they clamp it)
    for (i = [0:len(BOARD_SCREWS) - 1]) translate([BOARD_SCREWS[i][0], -BOARD_SCREWS[i][1], BOARD_SCREW_Z[i] - BOARD_HEAD[1]])
        cylinder(d = BOARD_HEAD[0], h = BOARD_HEAD[1], $fn = 32);
}
module backplane_envelope() { bp_pcb(); bp_switches(); bp_irleds(); bp_usbc(); bp_module(); bp_lra(); bp_zif(); bp_ics(); bp_screw_heads(); }
module board_envelope() { panel(); panel_fpc(); foam(); }

// INSERTION PATHS. Assembly order (README): caps into the face-down shell from
// the inside; then the board with the panel already plugged in and folded on
// it, lowered in from the back; then the deck onto the board; then the back plate. Each sweep is the part's
// outline (+ clearance) from where it sits down past the back opening; they
// must not meet the front shell (contacts at the posts are fine).
module down(z1, off = 0) { translate([0, 0, -1]) linear_extrude(z1 + 1) offset(delta = off) children(); }
module panel_path() {   // BL outline up to the LCD's top, lens outline up to the glass top, +0.15
    down(Z_LCD_BOT + LCD_T - EPS, 0.15) translate([LCD[0], -LCD[1] - LCD[3]]) square([LCD[2], LCD[3]]);
    down(Z_LENS_TOP - EPS, 0.15) rrect([GLASS[0], GLASS[1]], LENS[2], GLASS_C);
}
module board_path() {   // the board (+0.1) and everything on its top side, from below
    down(Z_BP_TOP - EPS, 0.1) bp2d();
    for (k = KEYS) down(Z_BP_TOP + SW_HEIGHT - EPS) translate([k[2][0], -k[2][1]]) square(SW_BODY, center = true);
    down(Z_BP_TOP + SW_HEIGHT - EPS) translate(S(PWR_KEY)) square(SW_BODY, center = true);
    for (x = IR_LED_X) down(Z_BP_TOP + 2.55 - EPS) translate([x - 1.15, -(IR_LED_TIP_Y + IR_LED_LEN + 0.6)]) square([2.3, IR_LED_LEN + 0.6 - 0.85]);
    down(Z_BP_TOP + USB_H - EPS) translate([USB_X - 4.47, -USB_FACE_Y]) square([8.94, 7.35]);
    for (q = PARTS) down(Z_BP_TOP + q[4] - EPS) translate([q[0] - q[2] / 2, -q[1] - q[3] / 2]) square([q[2], q[3]]);
}
module deck_path() {    // the deck (+0.1) and its bosses, from where it sits down past the back opening
    down(Z_BP_BOT - EPS, 0.1) deck2d();
}
module caps_path() {    // each cap's footprint from its flange's top down
    for (k = KEYS) if (k[1] != "ring" || k[0] == "UP") down(Z_FRONT_IN - (k[1] == "ok" ? CAP_FLANGE + DPAD_OK_DROP : 0) - 2 * EPS) projection() cap(k);
    down(Z_FRONT_IN - 2 * EPS) projection() pwr_cap();
}

module assembly() {
    color("dimgray") front_shell();
    color("white", 0.5) windows();
    color("gray") back_plate();
    color("white") for (k = KEYS) cap(k);
    color("black") for (k = KEYS) cap_label(k);
    color("white") pwr_cap();
    color("darkgreen", 0.7) backplane_envelope();
    color("navy", 0.5) board_envelope();
    color("orange", 0.6) battery();
    color("silver") deck();
}

// For the 3D viewer: export caps by colour group, in place.
names = [];
function in_list(x, l) = len([for (e = l) if (e == x) 1]) > 0;

if (part == "capgroup") { for (k = KEYS) if (in_list(k[0], names)) cap(k); }
else if (part == "pwrcap") pwr_cap();
else if (part == "internals") { board_envelope(); backplane_envelope(); battery(); deck(); }
// separate stand-ins for the 3D viewer's labelled X-ray view
else if (part == "v_panel") panel();
else if (part == "v_fpc") panel_fpc();
else if (part == "v_pcb") bp_pcb();
else if (part == "v_switches") bp_switches();
else if (part == "v_irleds") bp_irleds();
else if (part == "v_usbc") bp_usbc();
else if (part == "v_module") bp_module();
else if (part == "v_lra") bp_lra();
else if (part == "v_zif") bp_zif();
else if (part == "v_ics") bp_ics();
else if (part == "v_screws") bp_screw_heads();
else if (part == "v_battery") battery();
else if (part == "v_deck") deck();
else if (part == "front") front_print();
else if (part == "front_inplace") front_shell();
else if (part == "ir_window")       // on the bed flange down, outer face up
    translate([-IR_WINDOW[0], IR_WINDOW[3], WALL]) rotate([90, 0, 0]) ir_window();
else if (part == "sensor_window")   // on the bed pill face down, pegs up
    translate([-SENSOR_WIN[0], -SENSOR_WIN[1], H]) rotate([180, 0, 0]) sensor_window();
else if (part == "windows_inplace") windows();
else if (part == "back") back_plate();
else if (part == "deck") deck_print();
else if (part == "deck_inplace") deck();
else if (part == "caps") caps_print("body");
else if (part == "caps_labels") caps_print("labels");
else if (part == "labels_inplace") { for (k = KEYS) cap_label(k); }
else if (part == "caps_inplace") { for (k = KEYS) cap(k); pwr_cap(); }
else if (part == "assembly") assembly();
else if (part == "fit_test") {
    // Key area of the front shell only: check cap clearance, insert holes and
    // post height before printing the whole shell. Print face down.
    translate([0, 0, H]) rotate([180, 0, 0]) intersection() {
        front_shell();
        translate([-1, -L - 1, -1]) cube([W + 2, L - PWR_KEY[1] + 5, H + 2]);
    }
}
else if (part == "interference") {
    // everything here should be empty (zero-thickness contacts are fine)
    intersection() { union() { front_shell(); back_plate(); deck(); } union() { board_envelope(); battery(); backplane_envelope(); } }
}
else if (part == "deck_vs_shell") {
    // the deck against the front shell and the back plate: should be empty or
    // zero volume (its bosses rest on the plate's standoffs; the rails stop RAIL_GAP short)
    intersection() { deck(); union() { front_shell(); back_plate(); } }
}
else if (part == "windows_vs_parts") {
    // the windows (and the light pipe) against everything inside: should be empty
    intersection() { windows(); union() { back_plate(); deck(); board_envelope(); backplane_envelope(); battery(); for (k = KEYS) cap(k); pwr_cap(); } }
}
else if (part == "cap_interference") {
    intersection() { front_shell(); union() { for (k = KEYS) cap(k); pwr_cap(); } }
}
else if (part == "board_vs_bp") {
    // the panel and its folded FPC against the board's top-side parts: should be empty
    intersection() { panel(); union() { bp_switches(); bp_irleds(); bp_usbc(); bp_ics(); bp_zif(); } }
}
else if (part == "fpc_vs_parts") {
    // the folded FPC (and its parts area) against the board and its top-side parts (J1 aside): should be empty
    intersection() { panel_fpc(); union() { bp_pcb(); bp_switches(); bp_irleds(); bp_usbc(); bp_ics(); } }
}
else if (part == "panel_path") { intersection() { front_shell(); panel_path(); } }
else if (part == "board_path") { intersection() { front_shell(); board_path(); } }
else if (part == "deck_path") { intersection() { front_shell(); deck_path(); } }
else if (part == "caps_path") { intersection() { front_shell(); caps_path(); } }
else if (part == "foam_vs_parts") {
    // the foam pads against the FPC, the touch section and the board's top-side parts: should be empty
    intersection() { foam(); union() { panel_fpc(); bp_switches(); bp_irleds(); bp_ics(); bp_zif(); } }
}
else if (part == "rim_test") {
    // The shell's rim with all five posts/bosses on strips of the front plate:
    // print it face down, set the inserts and screw the bare board on, to check
    // the hole pitch (PETG shrink, insert wander) before the full shell.
    translate([0, 0, H]) rotate([180, 0, 0]) intersection() {
        front_shell();
        union() {
            translate([0, 0, -1]) linear_extrude(H + 2) difference() { outline2d(0.1); outline2d(-WALL - 1.2); }
            for (p = concat(KEY_HOLES, BOT_BOSSES)) {
                translate([p[0], -p[1], -1]) cylinder(d = BOSS_D + 2.0, h = H + 2);
                xw = p[0] < W / 2 ? 0 : W;            // a 4 mm strip of plate out to the nearer side wall
                translate([min(p[0], xw), -p[1] - 2, Z_FRONT_IN - 1]) cube([abs(xw - p[0]), 4, H]);
            }
        }
    }
}
else if (part == "cap_vs_parts") {
    // caps and their long stems against the backplane parts: should be empty
    intersection() { union() { for (k = KEYS) cap(k); pwr_cap(); } union() { backplane_envelope(); board_envelope(); } }
}
