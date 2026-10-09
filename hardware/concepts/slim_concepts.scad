// Width studies for the Bleep remote (render only, not the production model).
// Everything is laid out from params.scad and re-centred on the new width.
//   style 0: current, 65.2 mm (frame over the glass edge)
//   style 1: trimmed shell, wall + clearance beside the glass (~60.7 mm)
//   style 2: edge-to-edge glass across the full width (~57.4 mm)
//   style 3: like 2, with the screen right up to the top edge; the power key
//            and a sensor window (IR receiver + light sensor) in a row under it
//   style 4: trimmed shell (style 1) with the screen moved up as far as the
//            10 mm top corners allow, power key + sensor window under it
//   style 5: style 4 with 5 mm top corners, so the screen goes ~4 mm higher
//   style 6: trimmed, 5 mm top corners, screen 9.4 mm from the top so the
//            ESP32 antenna stays clear of it; power key + one sensor window
//            stay in the top strip, keys keep today's spacing to the screen
include <../enclosure/params.scad>
style = 0;
$fn = 72;

scheme = "concept";                     // "concept" or "arctic" (the viewer's colourways)
ARC = scheme == "arctic";
SHELL = ARC ? "#ffffff" : "#3a3c41";     // arctic white, lifted to read white under the render lighting
KEY = ARC ? "#b3b7bb" : "#55585e";
LIGHT = "#e9e6df";
ACCENT = "#5fb3c9";
OKC = ARC ? "#5ac4da" : LIGHT;          // OK key
DOT = ARC ? "#18191b" : LIGHT;          // legends (ring dots)
PWRC = ARC ? "#e5612b" : ACCENT;        // power key
GAP = ARC ? "#8a8e93" : "#151618";
GLASS = "#0b0c0e";

W0 = W;
WS = style == 0 ? W0 : (style == 1 || style >= 4) ? LENS[0] + 2 * 0.3 + 2 * 1.8 : LENS[0] + 0.9;
GY0 = style == 3 ? 0.45 : style == 4 ? 8.0 : style == 5 ? 4.0 : style == 6 ? 9.4 : LENS_C[1] - LENS[1] / 2;     // glass top edge
GB = GY0 + LENS[1];                                    // glass bottom edge
ROW = GB + 5.0;                                        // style 3: power key + sensor row
ky = style == 6 ? GY0 - (LENS_C[1] - LENS[1] / 2) : (style >= 3) ? (ROW + 2.25 + 4.0 + 15.5) - 121.56 : 0;   // keys move with the screen
LS = L + ky;
dx = (WS - W0) / 2;                     // re-centre everything on the new width
function P(p) = [p[0] + dx, -p[1]];
function PK(p) = P([p[0], p[1] + ky]);                 // key area (below the screen)
RT = style == 3 ? 0.6 : style == 2 ? 6 : style >= 5 ? 5 : R_TOP;      // style 3: square top end to match the rectangular glass            // tighter top corners when the glass runs edge to edge

module outline2d() {
    hull() {
        translate([RT, -RT]) circle(r = RT);
        translate([WS - RT, -RT]) circle(r = RT);
        translate([R_BOT, -LS + R_BOT]) circle(r = R_BOT);
        translate([WS - R_BOT, -LS + R_BOT]) circle(r = R_BOT);
    }
}

// body with a soft top edge
module body() {
    e = 1.6;
    color(SHELL) {
        linear_extrude(H - e) outline2d();
        for (i = [0:7]) {
            a = 90 * (i + 1) / 8;
            translate([0, 0, H - e + e * sin(a - 90 / 8)]) linear_extrude(e * (sin(a) - sin(a - 90 / 8)) + 0.01)
                offset(delta = -e * (1 - cos(a))) outline2d();
        }
    }
}

// the touch glass is a plain rectangle (0.3 mm corner radius)
module glass2d(grow = 0) {
    translate([(WS - LENS[0]) / 2 - grow, -GB - grow]) offset(r = LENS[2] + grow) offset(delta = -LENS[2])
        square([LENS[0], LENS[1]]);
}
module glass() {
    c = [WS / 2, -(GY0 + LENS[1] / 2)];
    edge = style == 2 || style == 3;
    gw = edge ? WS - 0.9 : LENS[0];
    top = edge ? H + 0.2 : Z_LENS_TOP;
    // glass, black print, then the active area with a hint of the UI
    translate([c[0], c[1], top - 0.6]) {
        if (edge) color(GLASS) translate([-c[0], -c[1], 0]) linear_extrude(0.6) glass2d();
        else color(GLASS) linear_extrude(0.6) offset(r = 0.6) square([gw - 1.2, LENS[1] - 1.2], center = true);
        aa_y = LENS[1] / 2 - VA_TOP - 0.5 - ACTIVE[1] / 2;
        translate([0, aa_y, 0.6]) {
            color("#1b2530") linear_extrude(0.02) square(ACTIVE, center = true);
            color("#2b3846") for (r = [0:2], col = [0:1]) translate([-11.5 + col * 23, 22 - r * 17, 0.02]) linear_extrude(0.02) offset(r = 2) square([17, 11], center = true);
            color(ACCENT) translate([-11.5, 22, 0.04]) linear_extrude(0.02) offset(r = 2) square([17, 11], center = true);
            color("#7f8a96") translate([0, -31, 0.02]) linear_extrude(0.02) offset(r = 1) square([40, 4], center = true);
        }
    }
    if (!edge) color(GAP) translate([c[0], c[1], Z_LENS_TOP - 0.02]) linear_extrude(H - Z_LENS_TOP + 0.03)
        difference() { square([LENS[0] - 2 * LIP_OVER + 1.0, LENS[1] - 2 * LIP_OVER + 1.0], center = true);
                       square([LENS[0] - 2 * LIP_OVER, LENS[1] - 2 * LIP_OVER], center = true); }
}

module key(h = CAP_PROTRUDE) { translate([0, 0, H - 0.3]) linear_extrude(h + 0.3) children(); }
module gap(d = 0.45) { color(GAP) translate([0, 0, H - 0.05]) linear_extrude(0.1) offset(delta = d) children(); }
module pill2d(w, h) { hull() for (s = [-1, 1]) translate([s * (w / 2 - h / 2), 0]) circle(d = h); }

module keys() {
    dp = PK([32.6, 121.56]);
    translate(dp) {
        gap() circle(r = 15.5);
        color(KEY) key() difference() { circle(r = 15.5); circle(r = 8.6); }
        for (a = [0:90:270]) rotate(a) color(DOT) translate([12.05, 0, H + CAP_PROTRUDE]) cylinder(r = 0.75, h = 0.05);
        color(OKC) key(CAP_PROTRUDE + 0.2) circle(r = 7.4);
    }
    for (x = [18.6, 32.6, 46.6]) translate(PK([x, 145.06])) { gap() circle(r = ROUND_R); color(KEY) key() circle(r = ROUND_R); }
    for (x = [18.6, 32.6, 46.6]) translate(PK([x, 157.06])) { gap() pill2d(PILL[0], PILL[1]); color(KEY) key() pill2d(PILL[0], PILL[1]); }
    translate(PK(VOL_C)) {
        gap() pill2d(VOL_BAR[0], VOL_BAR[1]);
        color(KEY) key() difference() { pill2d(VOL_BAR[0], VOL_BAR[1]); square([1.0, 20], center = true); }
    }
    pw = style == 6 ? [49.0, 6.2] : style >= 3 ? [46.6, ROW] : [49.0, 9.5];
    translate(P(pw)) { gap() pill2d(PWR_PILL[0], PWR_PILL[1]); color(PWRC) key() pill2d(PWR_PILL[0], PWR_PILL[1]); }
    if (style >= 3) {
        // one smoked window over the IR receiver and the light sensor
        translate(P(style == 6 ? [16.2, 6.2] : [18.6, ROW])) {
            color(GAP) translate([0, 0, H - 0.05]) linear_extrude(0.1) offset(delta = 0.3) pill2d(11.0, 4.0);
            color("#1c1f24") translate([0, 0, H - 0.04]) linear_extrude(0.1) pill2d(11.0, 4.0);
            color("#3a4450") translate([2.6, 0, H + 0.07]) cylinder(d = 2.8, h = 0.01);
            color("#3a4450") translate([-3.2, 0, H + 0.07]) cylinder(d = 1.4, h = 0.01);
        }
    } else
        for (s = SENSOR_HOLES) color(GAP) translate([P(s)[0], P(s)[1], H - 0.05]) cylinder(d = s[2], h = 0.1);
}

difference() {
    body();
    // screen window through the front frame (the frame overlaps the glass by LIP_OVER)
    if (style < 2 || style >= 4) { c = [WS / 2, -(GY0 + LENS[1] / 2)]; translate([c[0], c[1], Z_LENS_TOP - 0.01])
        linear_extrude(H) offset(r = 0.8) square([LENS[0] - 2 * LIP_OVER - 1.6, LENS[1] - 2 * LIP_OVER - 1.6], center = true); }
    // edge-to-edge: the glass sits in a shallow pocket across the full width
    if (style == 2 || style == 3) translate([0, 0, H - 0.4]) linear_extrude(2) glass2d(0.25);
}
glass();
keys();
