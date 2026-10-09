// D-pad design concepts for the Bleep remote. Each one uses the same five
// switches (UP/DOWN/LEFT/RIGHT 11 mm from the centre, OK in the middle) and
// sits between the screen (20.8 mm above the D-pad centre) and the row of
// round keys (23.5 mm below), on the real 65.2 mm wide face.
//   openscad -D variant=1 -o ring.png dpad_concepts.scad
variant = 1;
$fn = 96;

W = 65.2;
SHELL = "#34363a";
KEY = "#55585e";
LIGHT = "#e9e6df";
ACCENT = "#5fb3c9";
GAP = "#151618";
P = 1.4;          // how far the caps stand proud of the face

// ---------------------------------------------------------------- context
module face() {
    color(SHELL) translate([0, 0, -3]) linear_extrude(3)
        offset(r = 4) offset(delta = -4) translate([-W / 2, -36]) square([W, 70]);
    // screen (bottom part) and its black border
    color("#0d0e10") translate([-28.27, 20.8, 0]) cube([56.54, 14, 0.25]);
    color("#1d2a33") translate([-24.5, 24.0, 0.26]) cube([49.0, 11, 0.05]);
    // round keys below (BACK / HOME / MUTE)
    for (x = [-14, 0, 14]) translate([x, -23.5, 0]) {
        color(GAP) cylinder(r = 5.4, h = 0.05);
        color(KEY) dome_disc(5.0, P, 0.8);
    }
}

module gap2d(r) { color(GAP) linear_extrude(0.05) children(); }

// a disc with a softly rounded top edge
module dome_disc(r, h, e = 0.6) {
    rotate_extrude() hull() {
        square([r - e, h]);
        square([r, h - e]);
        translate([r - e, h - e]) circle(r = e, $fn = 24);
    }
}

// extrude a 2D shape with a rounded top edge (approximated by stacked insets)
module soft_extrude(h, e = 0.6, steps = 6) {
    linear_extrude(h - e) children();
    for (i = [0:steps - 1]) {
        a = 90 * (i + 1) / steps;
        translate([0, 0, h - e + e * sin(a - 90 / steps)])
            linear_extrude(e * (sin(a) - sin(a - 90 / steps)) + 0.001) offset(delta = -e * (1 - cos(a))) children();
    }
}

module tri2d(s) { polygon([[0, s * 0.6], [-s * 0.55, -s * 0.35], [s * 0.55, -s * 0.35]]); }
module chevron2d(s, t = 0.8) { difference() { tri2d(s); translate([0, -t]) tri2d(s); } }

module ok_text(h, size = 2.6, c = LIGHT) {
    color(c) translate([0, 0, h]) linear_extrude(0.05) text("OK", size = size, font = "Liberation Sans:style=Bold", halign = "center", valign = "center");
}

// ---------------------------------------------------------------- 1. ring (Chromecast / Google TV style)
module v_ring() {
    gap2d() difference() { circle(r = 16.3); circle(r = 7.6); }
    color(KEY) rotate_extrude() hull() {
        translate([8.6, 0]) square([7.2, 0.1]);
        translate([9.2, P - 0.6]) circle(r = 0.6, $fn = 24);
        translate([15.2, P - 0.6]) circle(r = 0.6, $fn = 24);
    }
    // four small dots mark the press points
    for (a = [0:90:270]) rotate(a) translate([12.2, 0, P]) color(LIGHT) cylinder(r = 0.65, h = 0.06);
    color(LIGHT) dome_disc(7.1, P + 0.2, 1.0);
}

// ---------------------------------------------------------------- 2. click wheel (iPod style)
module v_wheel() {
    gap2d() circle(r = 17.6);
    color(LIGHT) difference() {
        dome_disc(17.2, 0.9, 0.5);
        translate([0, 0, -1]) cylinder(r = 7.9, h = 5);
    }
    gap2d() circle(r = 7.9);
    color(LIGHT) dome_disc(7.4, 0.9, 0.5);
    // printed marks: arrows on the wheel, a thin ring on the button
    color("#8a8d93") for (a = [0:90:270]) rotate(a) translate([0, 12.7, 0.9]) linear_extrude(0.05) tri2d(2.4);
    color("#8a8d93") translate([0, 0, 0.9]) linear_extrude(0.05) difference() { circle(r = 5.6); circle(r = 5.2); }
}

// ---------------------------------------------------------------- 3. plus rocker (game pad)
module plus2d() {
    offset(r = 2.0) offset(delta = -2.0) union() {
        square([31, 10.5], center = true);
        square([10.5, 31], center = true);
    }
}
module v_plus() {
    gap2d() offset(delta = 0.5) plus2d();
    color(KEY) difference() {
        soft_extrude(P + 0.4, 0.8) plus2d();
        // shallow dish so the thumb rolls toward the centre
        translate([0, 0, 60 + P - 0.3]) sphere(r = 60, $fn = 160);
        translate([0, 0, -1]) cylinder(r = 5.9, h = 5);
    }
    for (a = [0:90:270]) rotate(a) color(LIGHT) translate([0, 11.6, P + 0.41]) linear_extrude(0.05) tri2d(2.6);
    gap2d() circle(r = 5.9);
    color(ACCENT) dome_disc(5.4, P + 0.2, 0.9);
}

// ---------------------------------------------------------------- 4. petals (four teardrop keys round the OK)
module petal2d() { hull() { translate([0, 12.3]) circle(r = 4.4); translate([0, 7.4]) circle(r = 1.6); } }
module v_petals() {
    for (a = [0:90:270]) rotate(a) {
        gap2d() offset(r = 0.5) petal2d();
        color(KEY) soft_extrude(P + 0.2, 0.9) petal2d();
        color(LIGHT) translate([0, 12.6, P + 0.25]) linear_extrude(0.05) chevron2d(2.6, 0.75);
    }
    gap2d() circle(r = 5.5);
    color(LIGHT) difference() {
        dome_disc(5.0, P + 0.6, 0.8);
        translate([0, 0, 22 + P + 0.2]) sphere(r = 22, $fn = 120);     // dished OK
    }
}

// ---------------------------------------------------------------- 5. squircle pad (Roku style)
module squircle2d(s, r) { offset(r = r) offset(delta = -r) square(s, center = true); }
module v_squircle() {
    gap2d() squircle2d(33.4, 10.5);
    color(KEY) difference() {
        soft_extrude(P, 0.7) squircle2d(32.4, 10);
        translate([0, 0, -1]) linear_extrude(5) squircle2d(14.8, 5);
    }
    for (a = [0:90:270]) rotate(a) color(ACCENT) translate([0, 12.2, P + 0.01]) linear_extrude(0.05) chevron2d(3.0, 0.8);
    gap2d() squircle2d(14.8, 5);
    color(LIGHT) soft_extrude(P + 0.2, 0.7) squircle2d(13.8, 4.6);
    ok_text(P + 0.21, 2.8, "#55585e");
}

// ---------------------------------------------------------------- 6. thumb nub (one tilting cap, press for OK)
module v_nub() {
    // fixed bezel on the face with four direction ticks
    color(SHELL) difference() {
        dome_disc(16.2, 0.8, 0.5);
        translate([0, 0, -1]) cylinder(r = 12.6, h = 5);
    }
    for (a = [0:90:270]) rotate(a) color(ACCENT) translate([0, 14.4, 0.8]) linear_extrude(0.05) square([0.9, 2.2], center = true);
    gap2d() circle(r = 12.6);
    // the nub: a concave thumb pad on a short neck
    color(LIGHT) difference() {
        union() {
            cylinder(r = 9.5, h = 1.0);
            translate([0, 0, 1.0]) cylinder(r1 = 11.6, r2 = 12.0, h = 1.6);
        }
        translate([0, 0, 2.6 + 40 - 0.9]) sphere(r = 40, $fn = 180);
    }
}

// ---------------------------------------------------------------- 7. split ring (four arcs, Apple-TV-like)
module arc2d(a0, a1, r0, r1) {
    intersection() {
        difference() { circle(r = r1); circle(r = r0); }
        polygon([[0, 0], [40 * cos(a0), 40 * sin(a0)], [40 * cos((a0 + a1) / 2), 40 * sin((a0 + a1) / 2)], [40 * cos(a1), 40 * sin(a1)]]);
    }
}
module v_arcs() {
    for (a = [0:90:270]) rotate(a) {
        gap2d() offset(r = 0.5) offset(r = 1) offset(delta = -1) arc2d(52, 128, 8.4, 16.0);
        color(KEY) soft_extrude(P, 0.7) offset(r = 1) offset(delta = -1) arc2d(52, 128, 8.4, 16.0);
        color(ACCENT) translate([0, 12.2, P + 0.01]) linear_extrude(0.05) tri2d(1.8);
    }
    gap2d() circle(r = 7.4);
    color(ACCENT) dome_disc(6.9, P + 0.3, 1.0);
}

// ---------------------------------------------------------------- 0. current design (four ring segments + OK)
module v_current() {
    for (a = [45:90:315]) rotate(a) {
        gap2d() offset(r = 0.5) arc2d(-44.0, 44.0, 7.7, 15.5);
    }
    for (a = [0:90:270]) rotate(a) {
        color(KEY) soft_extrude(P, 0.6) intersection() {
            difference() { circle(r = 15.5); circle(r = 7.7); }
            rotate(45) square(40);
            offset(delta = -0.6) rotate(45) square(40);
        }
        color(LIGHT) translate([11.6, 0, P + 0.01]) linear_extrude(0.05) rotate(-90) tri2d(2.2);
    }
    gap2d() circle(r = 5.7);
    color(KEY) dome_disc(5.2, P, 0.8);
    ok_text(P + 0.01, 2.4);
}

face();
if (variant == 0) v_current();
else if (variant == 1) v_ring();
else if (variant == 2) v_wheel();
else if (variant == 3) v_plus();
else if (variant == 4) v_petals();
else if (variant == 5) v_squircle();
else if (variant == 6) v_nub();
else if (variant == 7) v_arcs();
