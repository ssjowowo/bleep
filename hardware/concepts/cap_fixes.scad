// Concepts for the loose key caps (2026-10-17): round caps and the D-pad turn,
// the OK button floats and rolls. Each scene shows the real v1 shell and caps
// from ../enclosure/remote.scad next to one candidate fix.
//   openscad -D 'scene="socket_section"' -o socket.png cap_fixes.scad
// Scenes: now_section, socket_section, socket_under, ok_now_top, ok_big_top,
// ok_section, fork_under, hidden_key_under, hidden_key_section, collar_section,
// icons_now, icons_new
//
// The switch (TS-1187A) is drawn as a 5.1 x 5.1 mm metal body SW_BODY_T tall
// with a SW_PLUNGER_D plunger on top: ASSUMED, to be measured on the board.

include <../enclosure/remote.scad>
part = "none";
scene = "socket_section";
$fn = 64;

SW_BODY_T = 0.9;          // assumed: PCB to the top of the metal body
SW_PLUNGER_D = 2.6;       // assumed
Z_SW_BODY = Z_BP_TOP + SW_BODY_T;

C_SHELL = "#9aa3ad"; C_CAP = "#fbfaf6"; C_NEW = "#f29a3a";
C_LABEL = "#1b1b1b"; C_PCB = "#1f6b4a"; C_METAL = "#9a9ea3"; C_PLUNGER = "#2a2a2a";

HOME = KEYS[6];  RING = KEYS[0];  OKK = KEYS[2];
function kp(k) = S(k[2]);         // key centre, scad coords

// ---------------------------------------------------------------- context
module shell_box(c, r = 13) {   // the front shell around a point, for close-ups
    intersection() { front_shell(); translate([c[0] - r, c[1] - r, BACK_T]) cube([2 * r, 2 * r, H + 2]); }
}
module switch_at(p) {
    color(C_METAL) translate([p[0] - SW_BODY / 2, p[1] - SW_BODY / 2, Z_BP_TOP]) cube([SW_BODY, SW_BODY, SW_BODY_T]);
    color(C_PLUNGER) translate([p[0], p[1], Z_SW_BODY - EPS]) cylinder(d = SW_PLUNGER_D, h = Z_ACTUATOR - Z_SW_BODY + EPS);
}
module pcb_box(c, r = 13) { color(C_PCB) translate([c[0] - r, c[1] - r, Z_BP_BOT]) cube([2 * r, 2 * r, Z_BP_TOP - Z_BP_BOT]); }
module half_y(c) { translate([c[0] - 40, c[1], -5]) cube([80, 40, 40]); }        // keep y > c.y: cut face at y = c.y, facing front
module hx(c) { translate([c[0], c[1] - 40, -5]) cube([40, 80, 40]); }            // keep x > c.x: cut face facing left

// ---------------------------------------------------------------- fix 1: square socket over the switch body
SOCK_CLR = 0.15; SOCK_WALL = 0.8; SOCK_OVERLAP = 0.45;
module socket_cap(k) {
    p = kp(k);
    s_in = SW_BODY + 2 * SOCK_CLR; s_out = s_in + 2 * SOCK_WALL;
    z0 = Z_SW_BODY - SOCK_OVERLAP; z1 = Z_FRONT_IN - CAP_FLANGE;
    cap_generic(k, []);                                                  // same cap, no round stem
    translate([p[0], p[1], z0]) difference() {                           // skirt round the metal body
        linear_extrude(z1 - z0 + EPS) offset(r = 0.6) square(s_out - 1.2, center = true);
        translate([0, 0, -1]) linear_extrude(z1 - z0 - 0.6 + 1) square(s_in, center = true);
    }
    translate([p[0], p[1], Z_ACTUATOR - EPS]) cylinder(d = 2.2, h = z1 - Z_ACTUATOR + 2 * EPS);   // presses the plunger
}

// ---------------------------------------------------------------- fix 2: tighter OK
OK_R2 = DPAD_R_IN - 0.5;          // gap 1.2 -> 0.5 mm
OK_LIP2 = 1.0;                     // lip under the ring 0.4 -> 1.0 mm
module ok_cap2() {
    fl_top = Z_FRONT_IN - CAP_FLANGE - DPAD_OK_DROP;
    c = S(DPAD_C);
    translate([c[0], c[1], 0]) {
        translate([0, 0, fl_top]) cylinder(r = OK_R2, h = H + CAP_PROTRUDE - fl_top, $fn = 96);
        translate([0, 0, fl_top - CAP_FLANGE]) cylinder(r = DPAD_R_IN + OK_LIP2, h = CAP_FLANGE + EPS, $fn = 96);
        translate([0, 0, Z_ACTUATOR - EPS]) cylinder(d = 3.4, h = fl_top - CAP_FLANGE - Z_ACTUATOR + 2 * EPS);
    }
}

// ---------------------------------------------------------------- fix 3: ring fork round the right-hand post
POST = S(KEY_HOLES[1]);
FORK_CLR = 0.3; FORK_W = 1.2;
module ring_fork() {
    c = S(DPAD_C);
    d = POST - c; u = d / norm(d);
    a = c + u * (DPAD_R_OUT + 0.2);
    z0 = Z_FRONT_IN - CAP_FLANGE - 0.6; z1 = Z_FRONT_IN - 0.4;   // 0.4 below the wall so the ring can still rock
    translate([0, 0, z0]) linear_extrude(z1 - z0) difference() {
        union() {
            hull() { translate(a) circle(r = 1.6); translate(POST - u * (POST_D / 2 + FORK_CLR)) circle(r = 1.6); }
            translate(POST) circle(r = POST_D / 2 + FORK_CLR + FORK_W);
        }
        translate(POST) circle(r = POST_D / 2 + FORK_CLR);
        translate(c) circle(r = DPAD_R_OUT - 0.5);
    }
}

// ---------------------------------------------------------------- fix 4: hidden key in the wall's underside
KEY_W = 1.2; KEY_R1 = 7.2;  KEY_UP = 0.6;    // tab rises KEY_UP into a pocket in the wall
module hidden_tab(k) {
    p = kp(k);
    translate([p[0] - KEY_W / 2, p[1] + ROUND_R - 0.2, Z_FRONT_IN - CAP_FLANGE]) cube([KEY_W, KEY_R1 - ROUND_R + 0.2, CAP_FLANGE + KEY_UP]);
}
module hidden_pocket(k) {
    p = kp(k); g = 0.2;
    translate([p[0] - KEY_W / 2 - g, p[1] + ROUND_R, Z_FRONT_IN - 1]) cube([KEY_W + 2 * g, KEY_R1 - ROUND_R + g, 1 + KEY_UP + 0.25]);
}

// ---------------------------------------------------------------- fix 5: deep keyed guide collar
COLLAR_BOT = 9.3; COLLAR_T = 1.2;
module collar(k) {
    p = kp(k); r_in = ROUND_R + CAP_HOLE_CLR;
    translate([p[0], p[1], COLLAR_BOT]) difference() {
        cylinder(r = r_in + COLLAR_T, h = Z_FRONT_IN - COLLAR_BOT + EPS);
        translate([0, 0, -1]) cylinder(r = r_in, h = 10);
        translate([-0.7, 0, -1]) cube([1.4, r_in + 0.4, 10]);         // key slot (hidden, inside)
    }
}
module collar_cap(k) {
    p = kp(k);
    translate([p[0], p[1], 0]) difference() {
        union() {
            translate([0, 0, COLLAR_BOT]) cylinder(r = ROUND_R, h = H + CAP_PROTRUDE - COLLAR_BOT);
            translate([0, 0, COLLAR_BOT - 0.6]) cylinder(r = ROUND_R + CAP_HOLE_CLR + COLLAR_T, h = 0.6 + EPS);   // flange under the collar
            translate([-0.5, 0, COLLAR_BOT]) cube([1.0, ROUND_R + 0.25, Z_FRONT_IN - COLLAR_BOT - 0.2]);   // key rib
            translate([0, 0, Z_ACTUATOR - EPS]) cylinder(d = 3.4, h = COLLAR_BOT - 0.6 - Z_ACTUATOR + 2 * EPS);
        }
        translate([-p[0], -p[1], 0]) cap_engrave(k);
    }
}

// ---------------------------------------------------------------- new icons: thicker strokes, simpler shapes
ICON2_F = 0.9;          // ~5.4 mm across (was 4.5)
module icon2d_new(name) {
    f = ICON2_F; s = 0.95;           // stroke 0.95 mm (was 0.65), gaps >= 0.8 mm
    if (name == "@back") {
        r = 1.9 * f;
        translate([0, -0.3 * f]) {
            translate([0.6 * f, 0]) intersection() { annulus2d(r - s / 2, r + s / 2); translate([0, -5]) square(10); }
            translate([-0.9 * f, r - s / 2]) square([1.5 * f, s]);
            translate([-1.4 * f, -r - s / 2]) square([2.0 * f, s]);
            polygon([[-3.0 * f, r], [-0.9 * f, r + 1.5 * f], [-0.9 * f, r - 1.5 * f]]);
        }
    } else if (name == "@home") {          // solid house, no door
        scale(f) union() { polygon([[-3.0, 0.2], [3.0, 0.2], [0, 3.1]]); translate([-2.0, -2.8]) square([4.0, 3.1]); }
    } else if (name == "@mute") {          // speaker + a bold X, 0.8 mm apart
        translate([-0.9 * f, 0]) scale(f) {
            translate([-2.4, -1.1]) square([1.2, 2.2]);
            polygon([[-1.2, -1.1], [-1.2, 1.1], [0.6, 2.6], [0.6, -2.6]]);
        }
        for (a = [45, -45]) translate([1.8 * f, 0]) rotate(a) square([3.0 * f, 0.9], center = true);
    } else if (name == "@youtube") {       // solid play triangle
        scale(f) polygon([[-1.7, -2.1], [-1.7, 2.1], [2.2, 0]]);
    } else if (name == "@netflix") {
        scale(f) {
            translate([-1.8, -2.5]) square([1.15, 5.0]);
            translate([0.65, -2.5]) square([1.15, 5.0]);
            polygon([[-1.8, 2.5], [-0.65, 2.5], [1.8, -2.5], [0.65, -2.5]]);
        }
    } else if (name == "@plex") {
        scale(f) polygon([[-1.6, 2.6], [0.2, 2.6], [2.1, 0], [0.2, -2.6], [-1.6, -2.6], [0.3, 0]]);
    }
}
// a cap seen from the top, flat: the cap outline and its inlay. squish widens
// the inlay the way a squashed first layer does on the bed (the caps print face down).
module cap_top2d(k, new = false, squish = 0) {
    color(C_CAP) linear_extrude(0.5) cap2d(k);
    color(C_LABEL) translate([0, 0, 0.5]) linear_extrude(0.05) offset(delta = squish) intersection() {
        cap2d(k);
        if (new && len(k[4]) > 0 && k[4][0] == "@") translate(S(k[2])) icon2d_new(k[4]); else cap_label2d(k);
    }
}
ICON_KEYS = [KEYS[5], KEYS[6], KEYS[7], KEYS[8], KEYS[9], KEYS[10]];

// ---------------------------------------------------------------- scenes
// Each scene is a list of [piece, colour]. A piece is cut (where the scene
// cuts) and exported as its own STL by render_cap_fixes.sh; the view then
// imports the STLs and colours them (preview can't light cut faces).
piece = "";
view = false;
view_tilt = 0;
view_spin = 0;
stl_dir = "/o/stl";

function pieces(sc) =
    sc == "now_section"    ? [["shell", C_SHELL], ["pcb", C_PCB], ["swbody", C_METAL], ["plunger", C_PLUNGER], ["cap", C_CAP], ["label", C_LABEL]] :
    sc == "socket_section" ? [["shell", C_SHELL], ["pcb", C_PCB], ["swbody", C_METAL], ["plunger", C_PLUNGER], ["cap", C_NEW], ["label", C_LABEL]] :
    sc == "collar_section" ? [["shell", C_SHELL], ["pcb", C_PCB], ["swbody", C_METAL], ["plunger", C_PLUNGER], ["cap", C_NEW], ["label", C_LABEL]] :
    sc == "hidden_key_section" ? [["shell", [0.6, 0.64, 0.68, 0.4]], ["pcb", C_PCB], ["swbody", C_METAL], ["plunger", C_PLUNGER], ["cap", C_CAP], ["label", C_LABEL], ["tab", C_NEW]] :
    sc == "socket_under"   ? [["cap_old", C_CAP], ["cap", C_NEW]] :
    sc == "ok_now_top"     ? [["shell", C_SHELL], ["ring", C_CAP], ["ringlabel", C_LABEL], ["ok", C_CAP]] :
    sc == "ok_big_top"     ? [["shell", C_SHELL], ["ring", C_CAP], ["ringlabel", C_LABEL], ["ok", C_NEW]] :
    sc == "ok_now_section" ? [["shell", C_SHELL], ["pcb", C_PCB], ["swbody", C_METAL], ["plunger", C_PLUNGER], ["ring", C_CAP], ["ok", C_CAP]] :
    sc == "ok_big_section" ? [["shell", C_SHELL], ["pcb", C_PCB], ["swbody", C_METAL], ["plunger", C_PLUNGER], ["ring", C_CAP], ["ok", C_NEW]] :
    sc == "fork_under"     ? [["shell", C_SHELL], ["ring", C_CAP], ["fork", C_NEW], ["ok", C_CAP]] :
    sc == "hidden_key_under" ? [["shell", C_SHELL], ["cap", C_CAP], ["tab", C_NEW]] :
    [];

DP = S(DPAD_C);
RING_SW = [for (k = KEYS) if (k[1] == "ring" || k[1] == "ok") S(k[2])];
SECTIONS = ["now_section", "socket_section", "collar_section", "ok_now_section", "ok_big_section"];
OK_SCENES = ["ok_now_top", "ok_big_top", "ok_now_section", "ok_big_section"];
function centre(sc) = in_list(sc, OK_SCENES) ? DP :
                      sc == "fork_under" ? DP + [12, 0] : kp(HOME);

module raw_piece(sc, pc) {
    c = centre(sc);
    sws = in_list(sc, OK_SCENES) ? RING_SW : [c];
    if (pc == "shell") {
        r = sc == "fork_under" ? 21 : in_list(sc, OK_SCENES) ? 19 : sc == "hidden_key_under" ? 9 : 13;
        difference() {
            union() { shell_box(c, r); if (sc == "collar_section") collar(HOME); }
            if (sc == "hidden_key_section" || sc == "hidden_key_under") hidden_pocket(HOME);
        }
    }
    else if (pc == "pcb") { r = in_list(sc, OK_SCENES) ? 19 : 13; translate([c[0] - r, c[1] - r, Z_BP_BOT]) cube([2 * r, 2 * r, Z_BP_TOP - Z_BP_BOT]); }
    else if (pc == "swbody") for (p = sws) translate([p[0] - SW_BODY / 2, p[1] - SW_BODY / 2, Z_BP_TOP]) cube([SW_BODY, SW_BODY, SW_BODY_T]);
    else if (pc == "plunger") for (p = sws) translate([p[0], p[1], Z_SW_BODY - EPS]) cylinder(d = SW_PLUNGER_D, h = Z_ACTUATOR - Z_SW_BODY + EPS);
    else if (pc == "cap") {
        if (sc == "socket_section") socket_cap(HOME);
        else if (sc == "socket_under") translate([7, 0, 0]) socket_cap(HOME);
        else if (sc == "collar_section") collar_cap(HOME);
        else cap(HOME);
    }
    else if (pc == "cap_old") translate([-7, 0, 0]) cap(HOME);
    else if (pc == "label") cap_label(HOME);
    else if (pc == "tab") hidden_tab(HOME);
    else if (pc == "ring") cap(RING);
    else if (pc == "ringlabel") cap_label(RING);
    else if (pc == "ok") { if (sc == "ok_big_top" || sc == "ok_big_section") ok_cap2(); else cap(OKK); }
    else if (pc == "fork") ring_fork();
}
module cut_piece(sc, pc) {
    c = centre(sc);
    if (sc == "hidden_key_section") intersection() { raw_piece(sc, pc); hx(c); }
    else if (in_list(sc, SECTIONS)) intersection() { raw_piece(sc, pc); half_y(c); }
    else raw_piece(sc, pc);
}

if (piece != "") cut_piece(scene, piece);
else if (view)   // centred, and tipped by view_tilt so the cut faces turn up into the light
    rotate([view_tilt, 0, 0]) rotate([0, 0, view_spin]) translate([-centre(scene)[0], -centre(scene)[1], -9])
        for (p = pieces(scene)) color(p[1]) import(str(stl_dir, "/", scene, "_", p[0], ".stl"));
else if (scene == "icons_now" || scene == "icons_new" || scene == "icons_now_squish" || scene == "icons_new_squish") {
    nw = scene == "icons_new" || scene == "icons_new_squish";
    sq = (scene == "icons_now_squish" || scene == "icons_new_squish") ? 0.15 : 0;
    for (i = [0:len(ICON_KEYS) - 1]) {
        k = ICON_KEYS[i]; p = kp(k);
        translate([(i % 3) * 16 - p[0], -floor(i / 3) * 14 - p[1], 0]) cap_top2d(k, nw, sq);
    }
}
