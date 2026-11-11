// Volume-key placement studies (render only, not the production model).
// The main board is locked, so every key switch stays where it is; only the
// caps, the holes in the front shell and (in some styles) which function a
// switch does change. Switch centres (shell coords, y down from the top end):
//   row 2  y 141.43: x 16.37 / 30.37 / 44.37   (today MUTE / HOME / BACK)
//   apps   y 153.43: x 16.37 / 30.37 / 44.37   (today YT / NFLX / PLEX)
//   volume y 163.43: x 20.37 / 40.37           (today VOL- / VOL+)
//
//   style 0: today's split volume bar under the apps
//   style 1: two round keys over the volume switches
//   style 2: two wide pills over the volume switches
//   style 3: vertical volume rocker in the left column (row 2 + apps switches)
//   style 4: "- mute +" row of round keys right under the D-pad
//   style 5: like 4, as one three-segment bar; HOME | BACK split bar at the bottom
//
// openscad -D style=N -D 'vpart="shell"' -o shell.stl vol_concepts.scad
// vpart: shell, caps (caps_vol + caps_other), labels, dpad, ok, dpad_labels, pwr, windows, glass

include <../enclosure/remote.scad>
part = "none";             // don't let remote.scad render anything itself
style = 0;
vpart = "caps";
$fn = 64;

// only the D-pad keeps its production caps and hole
KEYS = [for (k = [["UP", "ring", [30.37, 106.93], 90, ""], ["RIGHT", "ring", [41.37, 117.93], 0, ""],
                  ["OK", "ok", [30.37, 117.93], 0, ""], ["LEFT", "ring", [19.37, 117.93], 180, ""],
                  ["DOWN", "ring", [30.37, 128.93], 270, ""]]) k];

R2 = 141.43; AP = 153.43; VY = 163.43;
C0 = 16.37; C1 = 30.37; C2 = 44.37; V0 = 20.37; V1 = 40.37;
BIG = [-100, -100, 200, 400];

// A key: [label, shape [cx, cy, w, h] (rounded, radius = half the short side),
//         cell [x0, y0, x1, y1] that clips the shape (split keys), label centre]
// Keys of one split group share a shape, which is also their hole.
function rnd(x, y, lab, d = 10) = [lab, [x, y, d, d], BIG, [x, y]];
function pil(x, y, lab, w = 11.5, h = 7) = [lab, [x, y, w, h], BIG, [x, y]];
function hsplit(sh, xs, labs) = [for (i = [0:len(labs) - 1])     // split across x at xs[]
    [labs[i], sh, [i == 0 ? -100 : xs[i - 1] + VOL_SPLIT / 2, -100, i == len(labs) - 1 ? 200 : xs[i] - VOL_SPLIT / 2, 400],
     [((i == 0 ? sh[0] - sh[2] / 2 : xs[i - 1]) + (i == len(labs) - 1 ? sh[0] + sh[2] / 2 : xs[i])) / 2, sh[1]]]];
function vsplit(sh, y, labs) = [                                 // split across y, labs top then bottom
    [labs[0], sh, [-100, -100, 200, y - VOL_SPLIT / 2], [sh[0], (sh[1] - sh[3] / 2 + y) / 2]],
    [labs[1], sh, [-100, y + VOL_SPLIT / 2, 200, 400], [sh[0], (sh[1] + sh[3] / 2 + y) / 2]]];

TODAY_ROW2 = [rnd(C0, R2, "@mute"), rnd(C1, R2, "@home"), rnd(C2, R2, "@back")];
TODAY_APPS = [pil(C0, AP, "@youtube"), pil(C1, AP, "@netflix"), pil(C2, AP, "@plex")];

VKEYS =
    style == 0 ? concat(TODAY_ROW2, TODAY_APPS, hsplit([C1, VY, 39, 7.5], [C1], ["-", "+"])) :
    style == 1 ? concat(TODAY_ROW2, TODAY_APPS, [rnd(V0, VY, "-", 10.5), rnd(V1, VY, "+", 10.5)]) :
    style == 2 ? concat(TODAY_ROW2, TODAY_APPS, [pil(V0, VY, "-", 17, 7.5), pil(V1, VY, "+", 17, 7.5)]) :
    style == 3 ? concat(vsplit([C0, (R2 + AP) / 2, 9, 21], (R2 + AP) / 2, ["+", "-"]),
                        [rnd(C1, R2, "@home"), rnd(C2, R2, "@back"),
                         pil(C1, AP, "@plex"), rnd(C2, AP, "@mute"),
                         pil(V0, VY, "@youtube", 16, 7), pil(V1, VY, "@netflix", 16, 7)]) :
    style == 4 ? concat([rnd(C0, R2, "-"), rnd(C1, R2, "@mute"), rnd(C2, R2, "+")], TODAY_APPS,
                        [rnd(V0, VY, "@home"), rnd(V1, VY, "@back")]) :
    /* 5 */      concat(hsplit([C1, R2, 40, 8.5], [(C0 + C1) / 2, (C1 + C2) / 2], ["-", "@mute", "+"]), TODAY_APPS,
                        hsplit([C1, VY, 39, 7.5], [C1], ["@home", "@back"]));

module shape2d(sh) { translate(S([sh[0], sh[1]])) hull() {
    r = min(sh[2], sh[3]) / 2;
    for (sx = [-1, 1], sy = [-1, 1]) translate([sx * (sh[2] / 2 - r), sy * (sh[3] / 2 - r)]) circle(r = r);
} }
module cell2d(c) { translate([c[0], -c[3]]) square([c[2] - c[0], c[3] - c[1]]); }
module vcap2d(k) { intersection() { shape2d(k[1]); cell2d(k[2]); } }

module vlabel2d(k) {
    translate(S(k[3])) {
        if (k[0][0] == "@") icon2d(k[0]);
        else { square([3.2, 0.75], center = true); if (k[0] == "+") square([0.75, 3.2], center = true); }
    }
}

TOP = H + CAP_PROTRUDE;
module vcap(k) {   // visual only: body from the inside of the front wall, 0.3 mm top round-off
    translate([0, 0, Z_FRONT_IN]) linear_extrude(TOP - 0.3 - Z_FRONT_IN) vcap2d(k);
    hull() {
        translate([0, 0, TOP - 0.3 - EPS]) linear_extrude(EPS) vcap2d(k);
        translate([0, 0, TOP - EPS]) linear_extrude(EPS) offset(delta = -0.3) vcap2d(k);
    }
}

if (vpart == "shell") difference() {
    front_shell();
    for (k = VKEYS) translate([0, 0, Z_FRONT_IN - 1]) linear_extrude(H) offset(delta = CAP_HOLE_CLR) shape2d(k[1]);
}
else if (vpart == "caps") for (k = VKEYS) vcap(k);
else if (vpart == "caps_vol") { for (k = VKEYS) if (k[0] == "+" || k[0] == "-") vcap(k); }
else if (vpart == "caps_other") { for (k = VKEYS) if (k[0] != "+" && k[0] != "-") vcap(k); }
else if (vpart == "labels") for (k = VKEYS) translate([0, 0, TOP - 0.2]) linear_extrude(0.22) intersection() { vlabel2d(k); offset(delta = -0.4) vcap2d(k); }
else if (vpart == "dpad") { for (k = KEYS) if (k[1] == "ring") cap(k); }
else if (vpart == "ok") { for (k = KEYS) if (k[1] == "ok") cap(k); }
else if (vpart == "dpad_labels") { for (k = KEYS) cap_label(k); }
else if (vpart == "pwr") pwr_cap();
else if (vpart == "windows") windows();
else if (vpart == "glass") panel();
