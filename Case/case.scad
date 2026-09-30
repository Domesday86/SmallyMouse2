// SmallyMouse2 case
//
// A two-part case: a base tray that the board sits in on four standoffs, and
// a lid that closes over it. Both parts split at the top face of the PCB, so
// the USB connector drops into a slot in the lid wall. Four M3 screws go up
// through the base, standoffs and board into pilot holes in the lid pillars,
// clamping everything together.
//
// Everything is positioned in the board coordinates of pcb.scad (origin at
// the board's lower-left corner, Z = 0 on its bottom face), so case features
// can be placed directly from the PCB model's measurements.
//
// Set `part` to choose what is rendered; from the command line:
//   openscad -o case-base.stl -D 'part="base"' case.scad

PCB_LIBRARY_ONLY = true;
include <pcb.scad>

part = "assembly";  // [assembly, base, lid, pcb]
explode = 0;        // Lift the lid in the assembly view

/* [Case] */
wall = 2.0;
floor_thickness = 2.0;
top_thickness = 2.0;
corner_radius = 3.0;
board_gap = 0.5;        // Clearance around the board edge
opening_gap = 0.5;      // Clearance around connectors through the lid
usb_gap = 0.3;          // Clearance around the USB shell in the lid slot
usb_protrusion = 0.5;   // USB mouth stands proud of the case wall

/* [Fixings] */
standoff_height = 4.0;  // Must clear PCB_PIN_TAIL
standoff_d = 6.0;
pillar_d = 5.5;         // Must fit in PCB_HOLE_KEEPOUT_D
screw_clearance_d = 3.4;
screw_pilot_d = 2.5;    // M3 self-tapping into plastic
screw_head_d = 6.0;
screw_head_depth = 3.0;

// Connectors reached through openings in the lid
LID_OPENINGS = ["J1", "J2", "J3", "J4", "J5", "J6"];

// ---------------------------------------------------------------------------
// Derived dimensions (board coordinates)

usb_box = pcb_part_box("P1");

case_min = [usb_box[0].x + usb_protrusion, -board_gap - wall];
case_max = [PCB_SIZE.x + board_gap + wall, PCB_SIZE.y + board_gap + wall];
inner_min = case_min + [wall, wall];
inner_max = case_max - [wall, wall];

z_bottom = -standoff_height - floor_thickness;
z_split = PCB_THICKNESS;
// Lid underside clears the USB shell; the IDC shroud reaches the lid's top
z_top = max(pcb_part_top("P1") + usb_gap + top_thickness, pcb_part_top("J3"));
z_ceiling = z_top - top_thickness;

assert(standoff_height > PCB_PIN_TAIL, "Standoffs too short for the pin tails");
assert(pillar_d <= PCB_HOLE_KEEPOUT_D, "Lid pillars overlap the hole keep-out");

echo(str("Case outside: ", case_max - case_min, " x ", z_top - z_bottom, " mm"));
echo(str("Screw: M3 x ", floor(z_ceiling - z_bottom - screw_head_depth - 1), " mm or shorter"));

// ---------------------------------------------------------------------------

// Rounded box between two XY corners, from z0 to z1.
module rounded_box(p0, p1, z0, z1, r) {
    translate([(p0.x + p1.x) / 2, (p0.y + p1.y) / 2, z0])
        cuboid([p1.x - p0.x, p1.y - p0.y, z1 - z0], rounding = r, edges = "Z",
               anchor = BOT);
}

module shell(z0, z1) {
    difference() {
        rounded_box(case_min, case_max, z0, z1, corner_radius);
        rounded_box(inner_min, inner_max, z0 - 1, z1 + 1, corner_radius - wall);
    }
}

module base() {
    difference() {
        union() {
            shell(z_bottom, z_split);
            rounded_box(case_min, case_max, z_bottom, z_bottom + floor_thickness,
                        corner_radius);
            for (h = PCB_HOLES) translate(h)
                cyl(d = standoff_d, h = standoff_height, anchor = TOP);
        }
        for (h = PCB_HOLES) translate(h) {
            cyl(d = screw_clearance_d, h = 50);
            translate([0, 0, z_bottom - 1])
                cyl(d = screw_head_d, h = screw_head_depth + 1, anchor = BOT);
        }
    }
}

module lid() {
    difference() {
        union() {
            shell(z_split, z_top);
            rounded_box(case_min, case_max, z_ceiling, z_top, corner_radius);
            for (h = PCB_HOLES) translate([h.x, h.y, PCB_THICKNESS])
                cyl(d = pillar_d, h = z_ceiling - PCB_THICKNESS, anchor = BOT);
        }
        // Blind pilot holes for the screws
        for (h = PCB_HOLES) translate([h.x, h.y, PCB_THICKNESS - 1])
            cyl(d = screw_pilot_d, h = z_top - PCB_THICKNESS, anchor = BOT);
        // USB slot, open at the bottom of the lid wall
        translate([0, 0, z_split - 1])
            cube_between([case_min.x - 1, usb_box[0].y - usb_gap],
                         [inner_min.x + 1, usb_box[1].y + usb_gap],
                         pcb_part_top("P1") + usb_gap - z_split + 1);
        for (ref = LID_OPENINGS) let (b = pcb_part_box(ref))
            translate([0, 0, z_ceiling - 1])
                cube_between(b[0] - [opening_gap, opening_gap],
                             b[1] + [opening_gap, opening_gap], top_thickness + 2);
    }
}

// Square-edged block between two XY corners, from z = 0 to z = h.
module cube_between(p0, p1, h) {
    translate([p0.x, p0.y, 0]) cube([p1.x - p0.x, p1.y - p0.y, h]);
}

if (part == "assembly") {
    color("#606060") base();
    smallymouse2_pcb();
    translate([0, 0, explode]) color("SteelBlue", 0.5) lid();
} else if (part == "base") {
    // Floor down on the print bed
    translate([0, 0, -z_bottom]) base();
} else if (part == "lid") {
    // Top down on the print bed
    translate([0, 0, z_top]) xrot(180) lid();
} else if (part == "pcb") {
    smallymouse2_pcb();
}
