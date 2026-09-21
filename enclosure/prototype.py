"""Generate a MorseBridge fit-check enclosure, in mm; not production-qualified.

Run with Python and the dependencies in requirements.txt installed:
  python prototype.py --output /path/to/output

The supplied XIAO case is the footprint/style reference, not a board-fit
reference. This is an editable rebuild, not a scaled copy of its mesh.
"""

import argparse
import json
from pathlib import Path

import manifold3d as m
import trimesh


INTERNAL_LENGTH = 38.0
INTERNAL_HEIGHT = 8.2
WIDTH = 24.0
WALL = 1.6
FLOOR = 1.6
ROOF = 1.6
LENGTH = INTERNAL_LENGTH + 2 * WALL
HEIGHT = INTERNAL_HEIGHT + FLOOR + ROOF
RADIUS = 3.0
LID_GAP = 0.25  # Per side; a locating lid, not a qualified snap fit.
SKIRT = 0.8
SKIRT_DEPTH = 2.0
BODY_HEIGHT = HEIGHT - ROOF
BOARD_X = WALL + 0.15
BOARD_Y = 3.0
BOARD_Z = 3.2
BOARD_LENGTH = 23.5
BOARD_WIDTH = 18.0
JACK_LENGTH = 14.0
JACK_WIDTH = 12.0
JACK_HEIGHT = 5.0
JACK_X = LENGTH - WALL - 0.15 - JACK_LENGTH
JACK_Y = (WIDTH - JACK_WIDTH) / 2
JACK_Z = FLOOR + 0.4
JACK_TERMINAL_HEIGHT = 2.5  # Provisional allowance above the housing, per photo.
JACK_BORE = 6.0  # User-specified opening diameter; printer compensation not added.
LED_BORE = 3.0
LED_X = 14.5  # Provisional: old slot centre until the LED position is measured.
LED_Y = WIDTH / 2
USB_WIDTH = 9.5
USB_HEIGHT = 3.4
USB_RADIUS = 1.5  # Measured rounded profile of the original XIAO case.
USB_CENTRE_Z = 5.4  # Retain our mounting elevation, not the original XIAO's.
USB_Z = USB_CENTRE_Z - USB_HEIGHT / 2


def box(x, y, z, dx, dy, dz):
    return m.Manifold.cube((dx, dy, dz)).translate((x, y, z))


def rounded(x, y, z, dx, dy, dz, radius):
    section = m.CrossSection.square((dx - 2 * radius, dy - 2 * radius))
    section = section.offset(radius, circular_segments=32)
    return section.extrude(dz).translate((x + radius, y + radius, z))


def export_mesh(solid, path):
    assert solid.status() == m.Error.NoError, (path, solid.status())
    mesh = solid.to_mesh()
    result = trimesh.Trimesh(
        vertices=mesh.vert_properties[:, :3], faces=mesh.tri_verts, process=True
    )
    assert result.is_watertight and result.is_winding_consistent, path
    assert result.volume > 0 and len(result.split()) == 1, path
    result.export(path)
    return {
        "file": path.name,
        "size_mm": result.extents.tolist(),
        "volume_mm3": float(result.volume),
        "triangles": len(result.faces),
        "watertight": bool(result.is_watertight),
    }


def make_parts():
    shell = rounded(0, 0, 0, LENGTH, WIDTH, BODY_HEIGHT, RADIUS)
    cavity = rounded(
        WALL, WALL, FLOOR, LENGTH - 2 * WALL, WIDTH - 2 * WALL,
        BODY_HEIGHT, RADIUS - WALL,
    )
    body = shell - cavity

    # Small edge ledges leave the underside open for wiring/components.
    for x in (4.0, 19.0):
        for y in (2.3, 20.2):
            body += box(x, y, FLOOR - 0.1, 2.5, 1.5, BOARD_Z - FLOOR + 0.1)
    for y in (1.5, 21.4):
        body += box(3, y, FLOOR - 0.1, 7, 1.1, 3.2)

    # Low saddle: the user's jack terminals and soldered wires face upward.
    for y in (JACK_Y - 0.8, JACK_Y + JACK_WIDTH - 0.4):
        body += box(JACK_X, y, FLOOR - 0.1,
                    JACK_LENGTH + 0.2, 1.2, JACK_Z - FLOOR + 0.1)
    for y in (JACK_Y - 1.1, JACK_Y + JACK_WIDTH + 0.3):
        body += box(JACK_X, y, FLOOR - 0.1,
                    JACK_LENGTH + 0.2, 0.8, 6.5)

    usb = rounded(0, 0, 0, USB_WIDTH, USB_HEIGHT, WALL + 2, USB_RADIUS)
    usb = usb.rotate((90, 0, 90)).translate(
        (-1, (WIDTH - USB_WIDTH) / 2, USB_Z)
    )
    barrel = m.Manifold.cylinder(
        WALL + 3, JACK_BORE / 2, circular_segments=64
    ).rotate((0, 90, 0)).translate(
        (LENGTH - WALL - 1, WIDTH / 2, JACK_Z + JACK_HEIGHT / 2)
    )
    body = body - usb - barrel

    lid = rounded(0, 0, BODY_HEIGHT, LENGTH, WIDTH, ROOF, RADIUS)
    inset = WALL + LID_GAP
    skirt_outer = rounded(
        inset, inset, BODY_HEIGHT - SKIRT_DEPTH,
        LENGTH - 2 * inset, WIDTH - 2 * inset,
        SKIRT_DEPTH + 0.1, RADIUS - inset,
    )
    skirt_inner = rounded(
        inset + SKIRT, inset + SKIRT, BODY_HEIGHT - SKIRT_DEPTH - 0.1,
        LENGTH - 2 * (inset + SKIRT), WIDTH - 2 * (inset + SKIRT),
        SKIRT_DEPTH + 0.3, 0.3,
    )
    # The short case puts the jack beneath the end skirt; relieve it locally.
    jack_relief = box(JACK_X - 0.1, JACK_Y - 1.2, FLOOR,
                      JACK_LENGTH + 0.4, JACK_WIDTH + 2.4, BODY_HEIGHT - FLOOR)
    lid += skirt_outer - skirt_inner - jack_relief
    led_hole = m.Manifold.cylinder(
        ROOF + 0.2, LED_BORE / 2, circular_segments=64
    ).translate((LED_X, LED_Y, BODY_HEIGHT - 0.1))
    lid -= led_hole
    # Keep this cut consistent if the jack elevation is adjusted later.
    lid -= barrel

    # Nominal component envelopes, not electrically complete CAD models.
    board = box(BOARD_X, BOARD_Y, BOARD_Z, BOARD_LENGTH, BOARD_WIDTH, 4.5)
    jack = box(JACK_X, JACK_Y, JACK_Z, JACK_LENGTH, JACK_WIDTH, JACK_HEIGHT)
    terminals = box(JACK_X, JACK_Y, JACK_Z + JACK_HEIGHT,
                    JACK_LENGTH, JACK_WIDTH, JACK_TERMINAL_HEIGHT)
    for name, solid in (("board envelope", board), ("jack housing", jack),
                        ("jack terminal allowance", terminals)):
        assert (body ^ solid).volume() < 1e-5, name + " intersects base"
        assert (lid ^ solid).volume() < 1e-5, name + " intersects lid"
    assert (body ^ lid).volume() < 1e-5, "lid intersects base"
    assert (board ^ jack).volume() < 1e-5, "component envelopes overlap"
    assert JACK_X - (BOARD_X + BOARD_LENGTH) >= 0.19
    assert abs((LENGTH - 2 * WALL) - 38.0) < 1e-6
    assert abs((BODY_HEIGHT - FLOOR) - 8.2) < 1e-6
    assert BODY_HEIGHT - JACK_Z - JACK_HEIGHT >= 0.19
    assert BODY_HEIGHT - JACK_Z - JACK_HEIGHT - JACK_TERMINAL_HEIGHT >= 0.29
    assert (body ^ barrel).volume() < 1e-5, "jack bore obstructed"
    assert (body ^ usb).volume() < 1e-5, "USB bore obstructed"
    # Preserve material in the rounded corners and close the old oversized slot.
    for y, z in (((WIDTH - USB_WIDTH) / 2 + 0.1, USB_Z + 0.1),
                 (WIDTH / 2 - 5.8, USB_CENTRE_Z)):
        material = box(0.4, y, z, 0.2, 0.1, 0.1)
        assert abs((body ^ material).volume() - material.volume()) < 1e-6
    assert (lid ^ led_hole).volume() < 1e-5, "LED bore obstructed"
    # The old long slot must be closed away from the new small circular opening.
    for x in (9.0, 19.0):
        closed_slot = box(x, LED_Y - 0.5, BODY_HEIGHT + 0.1, 1, 1, ROOF - 0.2)
        assert abs((lid ^ closed_slot).volume() - closed_slot.volume()) < 1e-5
    # Flat outer face on the build plate, skirt pointing up.
    print_lid = lid.rotate((180, 0, 0)).translate((0, WIDTH, HEIGHT))
    return body, print_lid


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    body, lid = make_parts()
    results = [
        export_mesh(body, args.output / "morsebridge_base_38inside_v4.stl"),
        export_mesh(lid, args.output / "morsebridge_lid_38inside_v4.stl"),
    ]
    report = {
        "project": "MorseBridge",
        "qualification": "FIT-CHECK PROTOTYPE ONLY; physical fit not verified",
        "assembled_size_mm": [LENGTH, WIDTH, HEIGHT],
        "wall_mm": WALL,
        "nominal_cavity_xy_mm": [LENGTH - 2 * WALL, WIDTH - 2 * WALL],
        "floor_to_lid_clearance_mm": BODY_HEIGHT - FLOOR,
        "jack_to_lid_clearance_mm": BODY_HEIGHT - JACK_Z - JACK_HEIGHT,
        "clearance_above_terminal_allowance_mm":
            BODY_HEIGHT - JACK_Z - JACK_HEIGHT - JACK_TERMINAL_HEIGHT,
        "board_to_jack_gap_mm": JACK_X - BOARD_X - BOARD_LENGTH,
        "lid_clearance_per_side_mm": LID_GAP,
        "jack_opening_diameter_mm": JACK_BORE,
        "usb_opening_width_height_mm": [USB_WIDTH, USB_HEIGHT],
        "usb_corner_radius_mm": USB_RADIUS,
        "usb_centre_height_mm": USB_CENTRE_Z,
        "led_opening_diameter_mm": LED_BORE,
        "led_opening_centre_xy_mm": [LED_X, LED_Y],
        "led_alignment": "Provisional old-slot centre; measure fitted LED before printing",
        "checks": "Watertight, positive volume, single shell, consistent winding; "
                  "nominal board/jack envelopes clear base and assembled lid",
        "parts": results,
    }
    (args.output / "mesh-checks.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
