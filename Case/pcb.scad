// SmallyMouse2 PCB model for case design
//
// A dimensional model of the assembled SmallyMouse2 board, built from the
// footprint placements in KiCAD/SmallyMouse2.kicad_pcb. It is meant for
// measuring a case against, not as a photorealistic render: component bodies
// are simple blocks at their nominal (datasheet-typical) sizes.
//
// Board coordinates (used by everything in this file and by case.scad):
//   origin  - lower-left corner of the board outline, bottom face of the PCB
//   X       - right, along the 52.3 mm edge
//   Y       - up the board, as seen in KiCad's top view
//   Z       - up from the bottom face; the top (component) face is at
//             PCB_THICKNESS
//
// Footprints are described in their own KiCad-local coordinates (Y down),
// so the numbers below can be checked directly against the .kicad_pcb and
// the footprints in KiCAD/SmallyMouse2.pretty.
//
// Open this file on its own to view the board. case.scad includes it as a
// library by setting PCB_LIBRARY_ONLY first.

include <BOSL2/std.scad>

$fa = 4;
$fs = 0.25;

// ---------------------------------------------------------------------------
// Board

// Edge.Cuts rectangle: (120.904, 80.264) - (173.228, 122.428) in KiCad
PCB_KICAD_ORIGIN = [120.904, 122.428];  // KiCad position of the board origin
PCB_SIZE = [52.324, 42.164];
PCB_THICKNESS = 1.6;

// M3 mounting holes (MK1-MK4). The keep-out is the ISO 14580 screw head
// courtyard on the top side.
PCB_HOLE_D = 3.2;
PCB_HOLE_KEEPOUT_D = 6.0;

// Through-hole pins stand proud of the bottom face by this much (2.54 mm
// headers: 3.0 mm tail below the plastic, less the board thickness). Leave at
// least this under the board.
PCB_PIN_TAIL = 3.0 - PCB_THICKNESS;

// ---------------------------------------------------------------------------
// Component sizes that are not in the KiCad data. These come from typical
// datasheet values; check against the parts actually fitted.

USB_A_HEIGHT = 7.0;         // Amphenol 87583-2010BLF shell height above board
IDC_HEIGHT = 8.9;           // 2x10 boxed (shrouded) header
HEADER_BODY_HEIGHT = 2.5;   // 2.54 mm pin header plastic
HEADER_PIN_HEIGHT = 8.5;    // 2.54 mm pin header, pin tip above board
TQFP_HEIGHT = 1.2;
XTAL_HEIGHT = 1.2;
C0805_HEIGHT = 0.85;
R0805_HEIGHT = 0.5;

// ---------------------------------------------------------------------------
// Footprint placements, from the .kicad_pcb:
//   [reference, type, KiCad position, KiCad rotation, pin count]

PCB_PARTS = [
    ["C1",  "C0805",  [132.334, 109.474],  90],
    ["C2",  "C0805",  [135.128, 109.474],  90],
    ["C3",  "C0805",  [141.478,  89.662], 180],
    ["C4",  "C0805",  [135.128, 100.076],  90],
    ["C5",  "C0805",  [136.652,  89.662], 180],
    ["C6",  "C0805",  [141.224, 110.998], 180],
    ["C7",  "C0805",  [149.352,  89.408],   0],
    ["C8",  "C0805",  [151.892, 115.062],   0],
    ["C9",  "C0805",  [163.830, 119.888], 180],
    ["C10", "C0805",  [161.290,  83.058],   0],
    ["R1",  "R0805",  [133.350,  96.774], 180],
    ["R2",  "R0805",  [133.350,  94.742], 180],
    ["R3",  "R0805",  [153.924, 111.252], -90],
    ["R4",  "R0805",  [126.365,  89.408], -90],
    ["IC1", "TQFP64", [145.288, 100.076],   0],
    ["Y1",  "XTAL",   [134.874, 114.046], 180],
    ["P1",  "USB_A",  [126.492, 101.346], -90],
    ["J1",  "HEADER", [130.302, 118.872],  90, 10],  // Expansion header
    ["J2",  "HEADER", [153.162,  86.614], 180,  2],  // Slow/Fast jumper
    ["J3",  "IDC20",  [162.306, 113.030],  90],      // BBC Micro user port
    ["J4",  "HEADER", [168.656, 113.030], 180, 10],  // Universal output
    ["J5",  "HEADER", [131.826, 104.648],  90,  2],  // Bootloader jumper
    ["J6",  "HEADER", [129.413,  90.170], 180,  2],  // Reset jumper
    ["TP1", "TP",     [132.080,  83.820],   0],      // JTAG test points
    ["TP2", "TP",     [137.668,  83.820],   0],
    ["TP3", "TP",     [143.256,  83.820],   0],
    ["TP4", "TP",     [146.050,  83.820], 180],
    ["TP5", "TP",     [134.874,  83.820], 180],
    ["TP6", "TP",     [140.462,  83.820], 180],
    ["TP7", "TP",     [148.844,  83.820],   0],
];

PCB_MOUNTING_HOLES_KICAD = [
    [124.460,  83.820],  // MK1
    [124.460, 118.618],  // MK2
    [169.672,  83.820],  // MK3
    [169.672, 118.618],  // MK4
];

// ---------------------------------------------------------------------------
// Coordinate conversion

// KiCad board position -> board coordinates
function kicad_xy(p) = [p.x - PCB_KICAD_ORIGIN.x, PCB_KICAD_ORIGIN.y - p.y];

// Footprint-local point -> board coordinates. KiCad's local Y points down and
// its rotation is counter-clockwise as seen from the top, so flipping Y turns
// it into an ordinary OpenSCAD rotation.
function fp_xy(at, rot, p) =
    let (c = cos(rot), s = sin(rot), q = [p.x, -p.y])
    kicad_xy(at) + [c * q.x - s * q.y, s * q.x + c * q.y];

// Places children, modelled in footprint-local coordinates with Z = 0 on the
// board's top face, onto the board.
module footprint(at, rot) {
    translate([each kicad_xy(at), PCB_THICKNESS]) zrot(rot) yflip() children();
}

PCB_HOLES = [for (h = PCB_MOUNTING_HOLES_KICAD) kicad_xy(h)];

// ---------------------------------------------------------------------------
// Part lookup, for positioning case features

function pcb_part(ref) = [for (p = PCB_PARTS) if (p[0] == ref) p][0];

// Local-coordinate outline [[x0, y0], [x1, y1]] and height above the board
// of each footprint type.
function part_outline(part) =
    let (type = part[1], pins = part[4])
      type == "C0805"  ? [[-1.0, -0.625], [1.0, 0.625]]
    : type == "R0805"  ? [[-1.0, -0.625], [1.0, 0.625]]
    : type == "TQFP64" ? [[-8, -8], [8, 8]]
    : type == "XTAL"   ? [[-2.5, -1.6], [2.5, 1.6]]
    : type == "USB_A"  ? [[-6.57, -1.07], [6.57, 12.84]]
    : type == "IDC20"  ? [[-5.08, -5.82], [27.94, 3.28]]
    : type == "HEADER" ? [[-1.27, -1.27], [1.27, (pins - 0.5) * 2.54]]
    : type == "TP"     ? [[-0.75, -0.75], [0.75, 0.75]]
    : assert(false, str("Unknown footprint type ", type));

function part_height(part) =
    let (type = part[1])
      type == "C0805"  ? C0805_HEIGHT
    : type == "R0805"  ? R0805_HEIGHT
    : type == "TQFP64" ? TQFP_HEIGHT
    : type == "XTAL"   ? XTAL_HEIGHT
    : type == "USB_A"  ? USB_A_HEIGHT
    : type == "IDC20"  ? IDC_HEIGHT
    : type == "HEADER" ? HEADER_PIN_HEIGHT
    : type == "TP"     ? 0.05
    : assert(false, str("Unknown footprint type ", type));

// Board-coordinate bounding box [[x0, y0], [x1, y1]] of a part, e.g.
// pcb_part_box("P1") for the USB connector (its mouth is the -X face).
function pcb_part_box(ref) =
    let (
        part = pcb_part(ref),
        o = part_outline(part),
        pts = [for (x = [o[0].x, o[1].x], y = [o[0].y, o[1].y])
                   fp_xy(part[2], part[3], [x, y])]
    )
    [[min([for (p = pts) p.x]), min([for (p = pts) p.y])],
     [max([for (p = pts) p.x]), max([for (p = pts) p.y])]];

// Height of a part's top above the bottom face of the board.
function pcb_part_top(ref) = PCB_THICKNESS + part_height(pcb_part(ref));

// Tallest part, above the bottom face of the board.
PCB_TOP = max([for (p = PCB_PARTS) PCB_THICKNESS + part_height(p)]);

// ---------------------------------------------------------------------------
// Model

PCB_COLOUR = "#1f6b2d";
COPPER_COLOUR = "gold";
METAL_COLOUR = "silver";
PLASTIC_COLOUR = "#202020";

// Box between two local-coordinate corners, from z0 to z1.
module _box(p0, p1, z0, z1) {
    translate([(p0.x + p1.x) / 2, (p0.y + p1.y) / 2, z0])
        cuboid([abs(p1.x - p0.x), abs(p1.y - p0.y), z1 - z0], anchor = BOT);
}

module _chip(height, body_colour) {
    color(body_colour) _box([-0.75, -0.625], [0.75, 0.625], 0, height);
    color(METAL_COLOUR)
        for (x = [-1, 1]) _box([x * 0.75, -0.625], [x * 1.0, 0.625], 0, height);
}

module _tqfp64() {
    color(PLASTIC_COLOUR) difference() {
        _box([-7, -7], [7, 7], 0.1, TQFP_HEIGHT);
        // Pin 1 marker
        translate([-5.5, -5.5, TQFP_HEIGHT]) cyl(d = 1, h = 0.2);
    }
    color(METAL_COLOUR)
        for (side = [0 : 3], i = [0 : 15])
            zrot(side * 90) translate([7.5, (i - 7.5) * 0.8, 0])
                cuboid([1, 0.37, 0.15], anchor = BOT);
}

module _xtal() {
    color("#c8b88a") _box([-2.5, -1.6], [2.5, 1.6], 0, 0.3);
    color(METAL_COLOUR) _box([-2.3, -1.4], [2.3, 1.4], 0.3, XTAL_HEIGHT);
}

// Amphenol 87583-2010BLF USB-A receptacle; the mouth faces local +Y.
module _usb_a() {
    color(METAL_COLOUR) difference() {
        _box([-6.57, -1.07], [6.57, 12.84], 0, USB_A_HEIGHT);
        _box([-6.0, 2.84], [6.0, 12.94], 1.0, USB_A_HEIGHT - 1.0);
    }
    color("white") _box([-5.6, 2.84], [5.6, 11.84], 3.4, 5.2);
    // Board-locating shell pegs through the 2.3 mm holes
    color(METAL_COLOUR)
        for (x = [-6.57, 6.57]) translate([x, 2.14, 0])
            cyl(d = 2.0, h = PCB_THICKNESS + 1, anchor = TOP);
    color(METAL_COLOUR)
        for (x = [-3.5, -1, 1, 3.5]) _box([x - 0.3, -2.5], [x + 0.3, -1.07], 0, 0.2);
}

// 2.54 mm pins, local Z = 0 on the board top
module _pins(positions) {
    color(COPPER_COLOUR)
        for (p = positions) translate([p.x, p.y, -PCB_THICKNESS - PCB_PIN_TAIL])
            cuboid([0.64, 0.64, HEADER_PIN_HEIGHT + PCB_THICKNESS + PCB_PIN_TAIL],
                   anchor = BOT);
}

module _header(pins) {
    color(PLASTIC_COLOUR)
        _box([-1.27, -1.27], [1.27, (pins - 0.5) * 2.54], 0, HEADER_BODY_HEIGHT);
    _pins([for (i = [0 : pins - 1]) [0, i * 2.54]]);
}

// 2x10 boxed header; pin 1 at the origin, second row at local Y = -2.54.
module _idc20() {
    color(PLASTIC_COLOUR) difference() {
        _box([-5.08, -5.82], [27.94, 3.28], 0, IDC_HEIGHT);
        _box([-2.78, -4.62], [25.64, 2.08], 2.0, IDC_HEIGHT + 0.1);
    }
    _pins([for (i = [0 : 9], y = [0, -2.54]) [i * 2.54, y]]);
}

module _test_point() {
    color(COPPER_COLOUR) _box([-0.75, -0.75], [0.75, 0.75], 0, 0.05);
}

module _part(part) {
    type = part[1];
    footprint(part[2], part[3]) {
        if (type == "C0805") _chip(C0805_HEIGHT, "#b08850");
        else if (type == "R0805") _chip(R0805_HEIGHT, PLASTIC_COLOUR);
        else if (type == "TQFP64") _tqfp64();
        else if (type == "XTAL") _xtal();
        else if (type == "USB_A") _usb_a();
        else if (type == "HEADER") _header(part[4]);
        else if (type == "IDC20") _idc20();
        else if (type == "TP") _test_point();
    }
}

module pcb_board() {
    usb = pcb_part("P1");
    color(PCB_COLOUR) difference() {
        cube([PCB_SIZE.x, PCB_SIZE.y, PCB_THICKNESS]);
        for (h = PCB_HOLES) translate(h) cyl(d = PCB_HOLE_D, h = 10);
        for (x = [-6.57, 6.57]) translate(fp_xy(usb[2], usb[3], [x, 2.14]))
            cyl(d = 2.3, h = 10);
    }
}

// The complete assembled board.
module smallymouse2_pcb() {
    pcb_board();
    for (p = PCB_PARTS) _part(p);
}

if (is_undef(PCB_LIBRARY_ONLY)) smallymouse2_pcb();
